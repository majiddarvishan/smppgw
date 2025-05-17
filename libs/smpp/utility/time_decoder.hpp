#include <chrono>
#include <fmt/chrono.h>
#include <fmt/core.h>

namespace pa::smpp
{
inline std::string abs_time_2_smpp(time_t abs_time)
{
    std::tm tm_date{};
    localtime_r(&abs_time, &tm_date);

    auto smpp_time = fmt::format("{:%y%m%d%H%M%S}", tm_date);

    // smpp_time += std::to_string(tm_date.tm_year % 100); // years after 1900
    // smpp_time += std::to_string(tm_date.tm_mon + 1);    // 0..11
    // smpp_time += std::to_string(tm_date.tm_mday);
    // smpp_time += std::to_string(tm_date.tm_hour);
    // smpp_time += std::to_string(tm_date.tm_min);
    // smpp_time += std::to_string(tm_date.tm_sec);

    smpp_time += "000+";

    return smpp_time;
}

inline time_t smpp_time_2_abs(const std::string& smpp_time)
{
    if (smpp_time.length() != 16)
    {
        throw std::runtime_error{ "time field with invalid length != 16" };
    }

    int year = std::stoi(smpp_time.substr(0, 2));
    int month = std::stoi(smpp_time.substr(2, 2));
    int day = std::stoi(smpp_time.substr(4, 2));
    int hour = std::stoi(smpp_time.substr(6, 2));
    int minute = std::stoi(smpp_time.substr(8, 2));
    int second = std::stoi(smpp_time.substr(10, 2));

    if (smpp_time.at(15) == 'R')
    {
        auto now = std::chrono::system_clock::now();
        now += std::chrono::years(year);
        now += std::chrono::months(month);
        now += std::chrono::days(day);
        now += std::chrono::hours(hour);
        now += std::chrono::minutes(minute);
        now += std::chrono::seconds(second);

        return std::chrono::system_clock::to_time_t(now);
    }

    if ((month == 0) || (month > 12) || (day == 0) || (day > 31) || (hour > 23) || (minute > 59) || (second > 59))
    {
        throw std::runtime_error{ "invalid time" };
    }

    if ((smpp_time.at(15) == '+') || (smpp_time.at(15) == '-'))
    {
        static constexpr int min_time_zone = -48;
        static constexpr int max_time_zone = 48;

        int time_zone = std::stoi(smpp_time.substr(13, 2));

        if (smpp_time.at(15) == '-')
        {
            time_zone *= -1;
        }
        else if (smpp_time.at(15) != '+')
        {
            throw std::runtime_error{ "invalid timezone" };
        }

        if (time_zone < min_time_zone || time_zone > max_time_zone)
        {
            throw std::runtime_error{ "invalid timezone" };
        }

        std::tm tm_date{};

        tm_date.tm_sec = second;    // 0..59
        tm_date.tm_min = minute;    // 0..59
        tm_date.tm_hour = hour;     // 0..23
        tm_date.tm_mday = day;      // 1.. 31
        tm_date.tm_mon = month - 1; // the tm_mon values are between 0 and 11
        tm_date.tm_year = year + 100;
        tm_date.tm_isdst = -1; //-1 means that we don't know the value of this field and the mktime

        time_t total_seconds = std::mktime(&tm_date);

        bool isdst = tm_date.tm_isdst;
        int tz = (abs(static_cast<int>(__timezone)) + (isdst * 3600)) / (15 * 60);
        total_seconds += (tz - time_zone) * 15 * 60;

        return total_seconds;
    }

    throw std::runtime_error{ "invalid vpf of time" };
}
} // namespace pa::smpp
