#pragma once

#include "logging.hpp"
#include <pa/config.hpp>

#include <deque>
#include <filesystem>
#include <fstream>
#include <sys/time.h>

namespace fs = std::filesystem;

namespace details
{
enum class file_mode
{
    text,
    binary
};

// begin_time = b
// end_time = e
// CreationTime = Y-M-d-h-m-s-S
// ClosingTime = Y-M-d-h-m-s-S
// Sequence = n

const std::string open_year_4_digit_index = "0";
const std::string open_year_2_digit_index = "1";
const std::string open_month_index = "2";
const std::string open_day_index = "3";
const std::string open_hour_index = "4";
const std::string open_minute_index = "5";
const std::string open_second_index = "6";
const std::string open_milli_second_index = "7";
const std::string close_year_4_digit_index = "8";
const std::string close_year_2_digit_index = "9";
const std::string close_month_index = "10";
const std::string close_day_index = "11";
const std::string close_hour_index = "12";
const std::string close_minute_index = "13";
const std::string close_second_index = "14";
const std::string close_milli_second_index = "15";
const std::string sequence_index = "16";

static std::string parse_format(const std::string& input)
{
    bool begin_time = true;

    std::string format;

    for (auto c = input.begin(); c != input.end();)
    {
        if (*c == '%')
        {
            switch (*(++c))
            {
                case 'Y':
                    format += "{" + (begin_time ? open_year_4_digit_index : close_year_4_digit_index) + ":04}";
                    break;

                case 'y':
                    format += "{" + (begin_time ? open_year_2_digit_index : close_year_2_digit_index) + ":02}";
                    break;

                case 'M':
                    format += "{" + (begin_time ? open_month_index : close_month_index) + ":02}";
                    break;

                case 'd':
                    format += "{" + (begin_time ? open_day_index : close_day_index) + ":02}";
                    break;

                case 'h':
                    format += "{" + (begin_time ? open_hour_index : close_hour_index) + ":02}";
                    break;

                case 'm':
                    format += "{" + (begin_time ? open_minute_index : close_minute_index) + ":02}";
                    break;

                case 's':
                    format += "{" + (begin_time ? open_second_index : close_second_index) + ":02}";
                    break;

                case 'S':
                    format += "{" + (begin_time ? open_milli_second_index : close_milli_second_index) + ":04}";
                    break;

                case 'n':
                    format += "{" + sequence_index + ":04}";
                    break;

                case 'b':
                    begin_time = true;
                    break;

                case 'e':
                    begin_time = false;
                    break;

                default:
                    break;
            }

            c++;
            continue;
        }

        format += *c;
        c++;
    }

    return format;
}
} // namespace details

class segmented_logger
{
  public:
    segmented_logger(pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config)
        : config_manager_{ config_manager }
        , config_{ config }
        , config_obs_enabled_replace_{ config_manager->on_replace(config->at("enabled"), std::bind_front(&segmented_logger::on_enabled_replace, this)) }
        , config_obs_buffer_size_replace_{ config_manager->on_replace(config->at("buffer_size"), std::bind_front(&segmented_logger::on_buffer_size_replace, this)) }
        , config_obs_records_threshold_replace_{ config_manager->on_replace(config->at("records_threshold"), std::bind_front(&segmented_logger::on_records_threshold_replace, this)) }
        , config_obs_time_threshold_replace_{ config_manager->on_replace(config->at("time_threshold"), std::bind_front(&segmented_logger::on_time_threshold_replace, this)) }
        , file_seq_no_{ 1 }
        , number_of_records_in_file_{ 0 }
    {
        is_enabled_ = config_->at("enabled")->get<bool>();

        auto conf = config_->at("file_mode")->get<std::string>();
        if (conf == "text")
        {
            file_mode_ = details::file_mode::text;
        }
        else if (conf == "binary")
        {
            file_mode_ = details::file_mode::binary;
        }
        else
        {
            LOG_CRITICAL("This file_mode({}) is not valid", conf);
            file_mode_ = details::file_mode::text;
        }

        parsed_format_ = details::parse_format(config_->at("file_name_format")->get<std::string>());
        // file_name_format_ = fmt::runtime(parsed_format_);

        create_path_ = config_->at("create_path")->get<std::string>();
        if (create_path_[create_path_.size() - 1] != '/')
        {
            create_path_ += '/';
        }

        close_path_ = config_->at("close_path")->get<std::string>();
        if (close_path_[close_path_.size() - 1] != '/')
        {
            close_path_ += '/';
        }

        buffer_size_ = config_->at("buffer_size")->get<uint32_t>();
        number_of_records_threshold_ = config_->at("records_threshold")->get<uint32_t>();
        time_threshold_ = std::chrono::seconds(config_->at("time_threshold")->get<uint32_t>());

        prepare_folders();
    }

    virtual ~segmented_logger()
    {
        close_file();
    }

    segmented_logger(const segmented_logger&) = delete;
    segmented_logger(segmented_logger&&) = delete;
    segmented_logger& operator=(const segmented_logger&) = delete;
    segmented_logger& operator=(segmented_logger&&) = delete;

    void record(const std::string& log)
    {
        if (is_enabled_)
        {
            records_.push_back(log);
            check_for_flush();
        }
    }

    void close_file()
    {
        if (!file_handler_.is_open())
            return;

        if ((file_mode_ == details::file_mode::text) && text_file_footer_.length())
        {
            file_handler_.write(text_file_footer_.c_str(), text_file_footer_.length());
        }

        file_handler_.close();

        LOG_INFO("LOG file ({}) is closed.", file_name_);

        std::string current_path = create_path_ + file_name_;

        struct timeval close_time_timeval;
        struct tm close_time_tm;
        gettimeofday(&close_time_timeval, nullptr);
        localtime_r(&close_time_timeval.tv_sec, &close_time_tm);

        auto close_file_name = fmt::format(
            fmt::runtime(parsed_format_), //file_name_format_,
            (creation_time_tm_.tm_year + 1900),
            (creation_time_tm_.tm_year - 100),
            (creation_time_tm_.tm_mon + 1),
            creation_time_tm_.tm_mday,
            creation_time_tm_.tm_hour,
            creation_time_tm_.tm_min,
            creation_time_tm_.tm_sec,
            (creation_time_timeval_.tv_usec / 100),
            (close_time_tm.tm_year + 1900),
            (close_time_tm.tm_year - 100),
            (close_time_tm.tm_mon + 1),
            close_time_tm.tm_mday,
            close_time_tm.tm_hour,
            close_time_tm.tm_min,
            close_time_tm.tm_sec,
            (close_time_timeval.tv_usec / 100),
            file_seq_no_);

        // copy file to close_folder that was specified in config file
        if (close_path_ != "")
        {
            std::string new_path = close_path_ + close_file_name;

            int result = rename(current_path.c_str(), new_path.c_str());

            if (result == 0)
            {
                // chmod(new_path.c_str(), 0776); // Set write permission on closed file.
                LOG_INFO("File was copied in close-folder successfully");
            }
            else
            {
                LOG_ERROR("Couldn't copy file to close-folder");
            }
        }

        number_of_records_in_file_ = 0;
        last_write_ = std::chrono::steady_clock::now();

        file_seq_no_++;
        if(file_seq_no_ >= 10000)
            file_seq_no_ = 1;
    }

    inline void set_header(const std::string& header)
    {
        text_file_header_ = header;
    }

    inline void set_footer(const std::string& footer)
    {
        text_file_footer_ = footer;
    }

    inline bool is_enabled() const
    {
        return is_enabled_;
    }

  private:
    void check_for_flush()
    {
        if (records_.size() >= buffer_size_)
            write_buffer_to_file();

        time_t currentTime = time(nullptr);
        struct tm currentTime_tm;
        localtime_r(&currentTime, &currentTime_tm);
        const auto since_last_write = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_write_);

        if ((number_of_records_in_file_ >= number_of_records_threshold_) ||
            (since_last_write >= time_threshold_) ||
            (currentTime_tm.tm_mday != creation_time_tm_.tm_mday))
        {
            write_buffer_to_file();
            close_file();

            number_of_records_in_file_ = 0;

            if (currentTime_tm.tm_mday != creation_time_tm_.tm_mday)
            {
                file_seq_no_ = 1;
            }

            LOG_INFO("File is closed.");
        }
    }

    void open_new_file()
    {
        auto current_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
        file_name_ = std::to_string(current_time.count()) + incomplete_file_extension_;

        std::string fullPath = create_path_ + file_name_;
        LOG_INFO("Creating  new file with name: {}", fullPath);

        file_handler_.open(fullPath.c_str(), std::ios::out | ((file_mode_ == details::file_mode::binary) ? std::ios::binary : std::ios::app));

        if (!file_handler_.is_open())
        {
            LOG_CRITICAL("Couldn't open a new log file({}).", fullPath);
            return;
        }

        gettimeofday(&creation_time_timeval_, nullptr);
        localtime_r(&creation_time_timeval_.tv_sec, &creation_time_tm_);

        if ((file_mode_ == details::file_mode::text) && text_file_header_.length())
        {
            file_handler_.write(text_file_header_.c_str(), text_file_header_.length());
            file_handler_.write("\n", 1);
        }
    }

    void write_buffer_to_file()
    {
        if (records_.empty())
            return;

        if (!file_handler_.is_open())
        {
            LOG_INFO("Open new file.");
            open_new_file();
        }

        LOG_INFO("LOG buffer flushed to file({})", file_name_);

        const auto chunk_begin = records_.begin();
        const auto chunk_end = records_.begin() + std::min(records_.size(), static_cast<size_t>(number_of_records_threshold_ - number_of_records_in_file_));

        std::for_each(chunk_begin, chunk_end, [&](const auto& str) {
            file_handler_.write(str.c_str(), str.length());
            if (file_mode_ == details::file_mode::text)
            {
                file_handler_.write("\n", 1);
            }

            number_of_records_in_file_++;
        });

        records_.erase(chunk_begin, chunk_end);

        file_handler_.flush();
    }

    void prepare_folders()
    {
        if (!fs::exists(create_path_))
        {
            if (!fs::create_directories(create_path_))
            {
                LOG_ERROR("Could not create path({}), switch to default path ./open", create_path_);
                create_path_ = "./open/";
                // mkdir(create_path_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                fs::create_directories(create_path_);
            }
        }

        if (!fs::exists(close_path_))
        {
            if (!fs::create_directories(close_path_))
            {
                LOG_ERROR("Could not create path({}), switch to default path ./close", close_path_);

                close_path_ = "./close/";
                // mkdir(close_path_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                fs::create_directories(close_path_);
            }
        }

        for (fs::path p : fs::directory_iterator(create_path_))
        {
            if (!fs::is_directory(p))
            {
                if (p.extension() == incomplete_file_extension_)
                {

                    std::string current_path = create_path_ + p.filename().string();

                    std::chrono::microseconds micros(stoll(p.stem().string()));
                    std::chrono::system_clock::time_point tp = std::chrono::system_clock::time_point(micros);

                    struct timeval creation_time_timeval;
                    auto duration = tp.time_since_epoch();
                    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
                    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration) - std::chrono::duration_cast<std::chrono::microseconds>(seconds);
                    creation_time_timeval.tv_sec = seconds.count();
                    creation_time_timeval.tv_usec = microseconds.count();

                    struct tm creation_time_tm;
                    localtime_r(&creation_time_timeval.tv_sec, &creation_time_tm);

                    struct timeval close_time_timeval;
                    struct tm close_time_tm;
                    gettimeofday(&close_time_timeval, nullptr);
                    localtime_r(&close_time_timeval.tv_sec, &close_time_tm);

                    auto close_file_name = fmt::format(
                        fmt::runtime(parsed_format_), //file_name_format_,
                        (creation_time_tm.tm_year + 1900),
                        (creation_time_tm.tm_year - 100),
                        (creation_time_tm.tm_mon + 1),
                        creation_time_tm.tm_mday,
                        creation_time_tm.tm_hour,
                        creation_time_tm.tm_min,
                        creation_time_tm.tm_sec,
                        (creation_time_timeval.tv_usec / 100),
                        (close_time_tm.tm_year + 1900),
                        (close_time_tm.tm_year - 100),
                        (close_time_tm.tm_mon + 1),
                        close_time_tm.tm_mday,
                        close_time_tm.tm_hour,
                        close_time_tm.tm_min,
                        close_time_tm.tm_sec,
                        (close_time_timeval.tv_usec / 100),
                        file_seq_no_);

                    file_seq_no_++;
                    std::string new_path = close_path_ + close_file_name;
                    fs::rename(current_path.c_str(), new_path.c_str());
                }
            }
        }
    }

    inline void on_enabled_replace(const std::shared_ptr<pa::config::node>& config)
    {
        is_enabled_ = config->get<bool>();
    }

    inline void on_buffer_size_replace(const std::shared_ptr<pa::config::node>& config)
    {
        buffer_size_ = config->get<uint32_t>();
    }

    inline void on_records_threshold_replace(const std::shared_ptr<pa::config::node>& config)
    {
        number_of_records_threshold_ = config->get<uint32_t>();
    }

    inline void on_time_threshold_replace(const std::shared_ptr<pa::config::node>& config)
    {
        time_threshold_ = std::chrono::seconds(config->get<uint32_t>());
    }

    pa::config::manager* config_manager_;
    std::shared_ptr<pa::config::node> config_;

    pa::config::manager::observer config_obs_enabled_replace_;
    pa::config::manager::observer config_obs_buffer_size_replace_;
    pa::config::manager::observer config_obs_records_threshold_replace_;
    pa::config::manager::observer config_obs_time_threshold_replace_;

    bool is_enabled_;

    details::file_mode file_mode_;

    int file_seq_no_ = 1; // Sequence Number of Current Log File

    std::string parsed_format_;
    // fmt::basic_runtime<fmt::char_t<std::string>> file_name_format_;
    // auto fmt::runtime(string_view s) -> runtime_format_string<>
    // std::string s = fmt::format(FMT_COMPILE("{}"), 42);



    std::string file_name_;     // Name of Current File
    std::fstream file_handler_; // Handler for Current File

    std::string create_path_; // The Path where Files are kept after creation
    std::string close_path_;  // The Path where Files will be put after closing

    unsigned int buffer_size_; // Maximum size of data in buffer

    // Creation time of current file
    struct timeval creation_time_timeval_;
    struct tm creation_time_tm_;

    std::string text_file_header_;
    std::string text_file_footer_;

    unsigned int number_of_records_in_file_;
    unsigned int number_of_records_threshold_;
    std::chrono::seconds time_threshold_; // a file can remain open up to this threshold time

    std::string incomplete_file_extension_ = ".incomp";

    std::deque<std::string> records_;

    std::chrono::time_point<std::chrono::steady_clock> last_write_{ std::chrono::steady_clock::now() };
};
