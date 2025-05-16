#pragma once

#include "logging.hpp"

#include <pa/config.hpp>

#include <boost/asio.hpp>
#include <chrono>
#include <deque>
#include <vector>

namespace io
{
namespace details
{
inline double standard_normal_generator()
{
    double u = ((double)(rand() + 1)) / RAND_MAX;
    double v = 6.283185307179586232 * (double(rand())) / RAND_MAX;

    return sqrt(-2 * log(u)) * cos(v);
}
} // namespace details

enum class flow_method
{
    disabled = 0,
    fixed_flow = 1,
    adaptive = 2,
    normal_distro = 3,
    credit = 4,
    limit_credit = 5,
};

class flow_control : public std::enable_shared_from_this<flow_control>
{
  public:
    using flow_handler = std::function<void(void)>;

    flow_control(boost::asio::io_context* io_context_ptr, pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config, const flow_handler& flow_handler)
        : config_{ config }
        , config_obs_max_packets_per_second_replace_{ config_manager->on_replace(
              config->at("max_packets_per_second"),
              std::bind_front(&flow_control::on_max_packets_per_second_replace, this)) }
        , config_obs_flow_method_replace_{ config_manager->on_replace(config->at("flow_method"), std::bind_front(&flow_control::on_flow_method_replace, this)) }
        , available_credit_{ 0 }
        , credit_arr_ind_{ 0 }
        , last_sec_{ 0 }
        , last_time_{ std::chrono::steady_clock::now() }
        , timer_(*io_context_ptr)
        , flow_handler_(flow_handler)
    {
        max_packets_per_second_ = config_->at("max_packets_per_second")->get<uint32_t>();

        auto conf = config_->at("flow_method")->get<std::string>();
        if (conf == "disabled")
        {
            flow_method_ = flow_method::disabled;
        }
        else if (conf == "fixed_flow")
        {
            flow_method_ = flow_method::fixed_flow;
        }
        else if (conf == "normal")
        {
            flow_method_ = flow_method::normal_distro;
        }
        else if (conf == "adaptive")
        {
            flow_method_ = flow_method::adaptive;
        }
        else if (conf == "credit")
        {
            flow_method_ = flow_method::credit;
        }
        else if (conf == "limit_credit")
        {
            flow_method_ = flow_method::limit_credit;
        }
        else
        {
            LOG_CRITICAL("This flow_method({}) is not valid", conf);
        }

        try
        {
            should_reject_packet_ = config_->at("should_reject_packet")->get<bool>();
            config_obs_should_reject_packet_replace_ = config_manager->on_replace(
                config->at("should_reject_packet"),
                std::bind_front(&flow_control::on_should_reject_packet_replace, this));
        }
        catch (const std::logic_error&)
        {
            should_reject_packet_ = false;
        }

        try
        {
            credit_windows_size_ = config_->at("credit_windows_size")->get<uint32_t>();
            config_obs_credit_windows_size_replace_ = config_manager->on_replace(
                config->at("credit_windows_size"),
                std::bind_front(&flow_control::on_credit_windows_size_replace, this));
        }
        catch (const std::logic_error&)
        {
            credit_windows_size_ = 5;
        }

        try
        {
            max_slippage_ = config_->at("max_slippage")->get<uint32_t>();
            config_obs_max_slippage_replace_ = config_manager->on_replace(config->at("max_slippage"), std::bind_front(&flow_control::on_max_slippage_replace, this));
        }
        catch (const std::logic_error&)
        {
            max_slippage_ = 0;
        }

        credit_arr_.clear();
        credit_arr_.resize(credit_windows_size_);

        elements_packet_count_.clear();
        elements_packet_count_.resize(credit_windows_size_);

        srand(time(nullptr));
    }

    virtual ~flow_control() = default;

    // return remain time in microsecond
    uint64_t check()
    {
        auto current_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

        switch (flow_method_)
        {
            case flow_method::normal_distro:
            {
                const auto jittered_time = static_cast<uint64_t>(current_time.count() + 300000 * details::standard_normal_generator());

                remove_past_data(current_time.count());

                if (flow_times_normal_.size() < max_packets_per_second_)
                {
                    flow_times_normal_.push_back(jittered_time);
                    return 0;
                }

                auto itr = flow_times_normal_.begin();
                return *itr + 1000000 - current_time.count();
            }

            case flow_method::fixed_flow:
            {
                remove_past_data(current_time.count());
                if (flow_times_fixed_.size() < max_packets_per_second_)
                {
                    flow_times_fixed_.push_back(current_time.count());

                    return 0;
                }

                auto itr = flow_times_fixed_.begin();
                return *itr + 1000000 - current_time.count();
            }

            case flow_method::adaptive:
            {
                credit_windows_size_ = (static_cast<uint64_t>(flow_times_fixed_.size()) * 1000000 / max_packets_per_second_) + 1;
                const uint32_t threshold = max_packets_per_second_ < 1000000 ? 10000000 / max_packets_per_second_ : max_packets_per_second_ / 100000;
                credit_windows_size_ = (credit_windows_size_ > threshold) ? threshold : credit_windows_size_;

                remove_past_data(current_time.count());

                if (flow_times_fixed_.size() <= (static_cast<uint64_t>(max_packets_per_second_) * credit_windows_size_ / 1000000))
                {
                    flow_times_fixed_.push_back(current_time.count());

                    return 0;
                }

                auto itr = flow_times_fixed_.begin();
                return *itr + credit_windows_size_ - current_time.count();
            }

            case flow_method::credit:
            {
                // rename to sec to increase readability of code
                auto current_time_sec = std::chrono::duration_cast<std::chrono::seconds>(current_time).count();

                if (last_sec_ == 0) // initialize the dedicated variables of this policy in first call of the function
                {
                    available_credit_ = max_packets_per_second_;
                    credit_arr_ind_ = 0;
                    last_sec_ = current_time_sec;

                    for (unsigned i = 0; i < credit_windows_size_; i++)
                    {
                        credit_arr_[i] = 0;
                    }

                    npack_in_sec_ = 0;
                }
                else if (last_sec_ != static_cast<uint64_t>(current_time_sec)) // indicate second part of system time is changed
                {
                    if ((current_time_sec - last_sec_) >= credit_windows_size_)
                    {
                        available_credit_ = max_packets_per_second_ * credit_windows_size_;
                        credit_arr_ind_ = 0;

                        for (unsigned i = 0; i < credit_windows_size_; i++)
                        {
                            credit_arr_[i] = max_packets_per_second_;
                        }
                    }
                    else if (((current_time_sec - last_sec_) > 1) && ((current_time_sec - last_sec_) < credit_windows_size_))
                    {
                        for (unsigned i = 0; i < current_time_sec - last_sec_ - 1; i++)
                        {
                            available_credit_ -= credit_arr_[credit_arr_ind_];
                            available_credit_ += max_packets_per_second_;
                            credit_arr_[credit_arr_ind_] = max_packets_per_second_;
                            credit_arr_ind_++;

                            if (credit_arr_ind_ >= credit_windows_size_)
                            {
                                credit_arr_ind_ = 0;
                            }
                        }
                    }

                    last_sec_ = current_time_sec;                                   // update variable of lastSec
                    available_credit_ += (max_packets_per_second_ - npack_in_sec_); // update available credit

                    if (npack_in_sec_ <= max_packets_per_second_)
                    {
                        available_credit_ -= credit_arr_[credit_arr_ind_];
                        credit_arr_[credit_arr_ind_] = max_packets_per_second_ - npack_in_sec_;
                        credit_arr_ind_++;
                    }
                    else
                    {
                        uint32_t extra_used_credit = npack_in_sec_ - max_packets_per_second_;
                        uint32_t ind = credit_arr_ind_;

                        int credit = 0;

                        while (extra_used_credit > 0)
                        {
                            // if(credit > mMaxLoanedCredit)
                            //{
                            // break;
                            // }

                            if (extra_used_credit > credit_arr_[ind])
                            {
                                extra_used_credit -= credit_arr_[ind];
                                credit_arr_[ind] = 0;
                                ind++;

                                if (ind >= credit_windows_size_)
                                {
                                    ind = 0;
                                }

                                credit++;
                            }
                            else
                            {
                                credit_arr_[ind] -= extra_used_credit;
                                extra_used_credit = 0;
                            }
                        }

                        available_credit_ -= credit_arr_[credit_arr_ind_];
                        credit_arr_[credit_arr_ind_] = 0;
                        credit_arr_ind_++;
                    }

                    if (credit_arr_ind_ >= credit_windows_size_)
                    {
                        credit_arr_ind_ = 0;
                    }

                    npack_in_sec_ = 0;
                }

                //--------------

                // remove_past_data ( timeInMicroseconds );
                // uint32_t maxPacketsWithCredit =  max_packets_per_second_ * credit_windows_size_;
                // r = ( flow_times_fixed_.size() <  available_credit_ );

                if (npack_in_sec_ < available_credit_)
                {
                    // flow_time_list_entry entry;
                    // entry.mByteCount = packetLength;
                    // entry.flow_time_ = timeInMicroseconds;
                    // mTotalBytes += packetLength;
                    // available_credit_ -= 1;
                    npack_in_sec_ += 1;
                    // flow_times_fixed_.push_back ( entry );

                    return 0;
                }

                auto itr = flow_times_fixed_.begin();
                return *itr + 1000000 - current_time.count();
            }

            case flow_method::limit_credit:
            {
                const auto now = std::chrono::steady_clock::now();

                auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last_time_).count();

                for (auto i = 0; i < diff; i++)
                {
                    window_total_packets_ -= elements_packet_count_.front();
                    elements_packet_count_.pop_front();
                    elements_packet_count_.push_back(0);
                }

                if (diff)
                {
                    last_time_ = now;
                }

                if ((elements_packet_count_.back() >= (max_packets_per_second_ + max_slippage_)) || (window_total_packets_ >= (max_packets_per_second_ * credit_windows_size_)))
                {
                    return 1000000;
                }

                elements_packet_count_.back()++;
                window_total_packets_++;

                return 0;
            }

            case flow_method::disabled:
            default:
                return 0;
        }

        return 0;
    }

    inline bool drop_packet() const
    {
        return should_reject_packet_;
    }

    void wait(std::chrono::time_point<std::chrono::steady_clock> timepoint)
    {
        timer_.expires_at(timepoint);
        timer_.async_wait([this, wptr = this->weak_from_this()](boost::system::error_code ec) {
            if (!wptr.expired())
            {
                if (ec == boost::asio::error::operation_aborted)
                    return;

                if (ec)
                    throw std::runtime_error("io::expirator::async_wait() " + ec.message());

                flow_handler_();
            }
        });
    }

  private:
    void remove_past_data(uint64_t time_in_micro)
    {
        switch (flow_method_)
        {
            case flow_method::adaptive:
            {
                for (auto itr = flow_times_fixed_.begin(); itr != flow_times_fixed_.end();)
                {
                    if (time_in_micro - *itr > credit_windows_size_)
                    {
                        itr = flow_times_fixed_.erase(itr);
                    }
                    else
                    {
                        break;
                    }
                }

                break;
            }

            case flow_method::fixed_flow:
            {
                for (auto itr = flow_times_fixed_.begin(); itr != flow_times_fixed_.end();)
                {
                    if (time_in_micro - *itr > 1000000) // one second
                    {
                        itr = flow_times_fixed_.erase(itr);
                    }
                    else
                    {
                        break;
                    }
                }

                break;
            }

            case flow_method::normal_distro:
            {
                while (!flow_times_normal_.empty())
                {
                    auto itr = flow_times_normal_.begin();

                    if (*itr < time_in_micro - 1000000)
                    {
                        flow_times_normal_.erase(itr);
                    }
                    else
                    {
                        break;
                    }
                }

                break;
            }

            case flow_method::credit:
            case flow_method::disabled:
            default:
                break;
        }
    }

    void on_max_packets_per_second_replace(const std::shared_ptr<pa::config::node>& config)
    {
        max_packets_per_second_ = config->get<uint32_t>();
    }

    void on_flow_method_replace(const std::shared_ptr<pa::config::node>& config)
    {
        auto conf = config->get<std::string>();
        if (conf == "disabled")
        {
            flow_method_ = flow_method::disabled;
        }
        else if (conf == "fixed_flow")
        {
            flow_method_ = flow_method::fixed_flow;
        }
        else if (conf == "normal")
        {
            flow_method_ = flow_method::normal_distro;
        }
        else if (conf == "adaptive")
        {
            flow_method_ = flow_method::adaptive;
        }
        else if (conf == "credit")
        {
            flow_method_ = flow_method::credit;
        }
        else if (conf == "limit_credit")
        {
            flow_method_ = flow_method::limit_credit;
        }
        else
        {
            LOG_CRITICAL("This flow_method({}) is not valid", conf);
        }
    }

    void on_should_reject_packet_replace(const std::shared_ptr<pa::config::node>& config)
    {
        should_reject_packet_ = config->get<bool>();
    }

    void on_credit_windows_size_replace(const std::shared_ptr<pa::config::node>& config)
    {
        credit_windows_size_ = config->get<uint32_t>();

        credit_arr_.clear();
        credit_arr_.resize(credit_windows_size_);

        elements_packet_count_.clear();
        elements_packet_count_.resize(credit_windows_size_);
    }

    void on_max_slippage_replace(const std::shared_ptr<pa::config::node>& config)
    {
        max_slippage_ = config->get<uint32_t>();
    }

    std::shared_ptr<pa::config::node> config_;

    pa::config::manager::observer config_obs_max_packets_per_second_replace_;
    pa::config::manager::observer config_obs_flow_method_replace_;
    pa::config::manager::observer config_obs_should_reject_packet_replace_;
    pa::config::manager::observer config_obs_credit_windows_size_replace_;
    pa::config::manager::observer config_obs_max_slippage_replace_;

    uint32_t max_packets_per_second_;
    flow_method flow_method_;
    bool should_reject_packet_;

    std::deque<uint64_t> flow_times_normal_; // used in normal method (in microseconds)
    std::deque<uint64_t> flow_times_fixed_;  // used in fixed and adaptive method (in microseconds)

    uint32_t available_credit_ = 0;
    uint32_t credit_arr_ind_ = 0;
    uint32_t npack_in_sec_;
    uint64_t last_sec_ = 0;

    std::vector<uint32_t> credit_arr_;

    unsigned credit_windows_size_ = 5;
    unsigned max_slippage_ = 0;
    unsigned window_total_packets_ = 0;
    std::deque<unsigned> elements_packet_count_;

    std::chrono::time_point<std::chrono::steady_clock> last_time_;

    boost::asio::steady_timer timer_;
    flow_handler flow_handler_{};
};
} // namespace io