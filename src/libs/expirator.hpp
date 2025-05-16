#pragma once

#include <boost/asio.hpp>

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>

#include <optional>

namespace io
{
template<typename Tkey, typename Tinfo>
class expirator : public std::enable_shared_from_this<expirator<Tkey, Tinfo>>
{
  public:
    using expiry_handler = std::function<void(Tkey, Tinfo)>;

  private:
    boost::asio::steady_timer timer_;
    const std::chrono::nanoseconds period_{};
    uint64_t index_{};
    using bimap_t = boost::bimap<boost::bimaps::unordered_multiset_of<unsigned int>, boost::bimaps::unordered_set_of<Tkey>, boost::bimaps::with_info<Tinfo>>;
    using value_t = typename bimap_t::value_type;
    bimap_t index_keys_;
    const expiry_handler expiry_handler_{};

  public:
    expirator(boost::asio::io_context* io_context_ptr, std::chrono::nanoseconds period, expiry_handler expiry_handler)
        : timer_(*io_context_ptr)
        , period_(period)
        , expiry_handler_(std::move(expiry_handler))
    {
    }

    expirator(const expirator&) = delete;
    expirator& operator=(const expirator&) = delete;
    expirator(expirator&&) = delete;
    expirator& operator=(expirator&&) = delete;
    ~expirator() = default;

    void start()
    {
        do_set_timer();
    }

    void add(Tkey key, std::chrono::nanoseconds expiration_count)
    {
        index_keys_.insert(value_t(index_ + expiration_count.count() / period_.count(), key));
    }

    void add(Tkey key, std::chrono::nanoseconds expiration_count, Tinfo info)
    {
        index_keys_.insert(value_t(index_ + expiration_count.count() / period_.count(), key, info));
    }

    std::optional<Tinfo> get_info(Tkey key)
    {
        auto it = index_keys_.right.find(key);
        if (it != index_keys_.right.end())
            return it->info;

        return {};
    }

    bool remove(Tkey key)
    {
        return index_keys_.right.erase(key);
    }

    void expire_all()
    {
        std::for_each(index_keys_.left.begin(), index_keys_.left.end(), [this](auto const& it) {
            expiry_handler_(it.second, it.info);
            });

        index_keys_.clear();

        index_ = 0;
    }

  private:
    void on_timer()
    {
        auto range = index_keys_.left.equal_range(index_);
        std::for_each(range.first, range.second, [this](auto const& it) { expiry_handler_(it.second, it.info); });

        index_keys_.left.erase(index_);

        index_++;
    }

    void do_set_timer()
    {
        timer_.expires_after(period_);
        timer_.async_wait([this, wptr = this->weak_from_this()](std::error_code ec) {
            if (!wptr.expired())
            {
                if (ec)
                    throw std::runtime_error("io::expirator::async_wait() " + ec.message());

                on_timer();
                do_set_timer();
            }
        });
    }
};
} // namespace io