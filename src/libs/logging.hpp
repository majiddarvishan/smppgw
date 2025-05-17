#pragma once

#include <pa/config.hpp>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/bin_to_hex.h>

class logging
{
    pa::config::manager::observer config_level_replace_;

public:
    logging(pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config)
        : config_level_replace_{config_manager->on_replace(config, std::bind_front(&logging::on_level_replace))}
    {
        std::vector<spdlog::sink_ptr> sink_list{};

        if((config->at("output_mode")->get<std::string>() == "console") || (config->at("output_mode")->get<std::string>() == "both"))
        {
            sink_list.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_st>());
        }

        if((config->at("output_mode")->get<std::string>() == "file") || (config->at("output_mode")->get<std::string>() == "both"))
        {
            sink_list.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_st>(
                                    config->at("file_name")->get<std::string>(),
                                    config->at("max_file_size")->get<std::uint32_t>() * 1024 * 1024,
                                    config->at("max_files")->get<std::uint32_t>(),
                                    true));
        }

        auto logger = std::make_shared<spdlog::logger>("main", std::begin(sink_list), std::end(sink_list));
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("%t [%d %b %Y %H:%M:%S] [%l] (%s,%#) - %^%v%$");
        spdlog::set_level(spdlog::level::from_str(config->at("level")->get<std::string>()));
    }

    static void on_level_replace(const std::shared_ptr<pa::config::node>& config)
    {
        spdlog::set_level(spdlog::level::from_str(config->at("level")->get<std::string>()));
    }
};

#define LOG_TRACE(...)\
    spdlog::log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::trace, __VA_ARGS__)

#define LOG_DEBUG(...)\
    spdlog::log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::debug, __VA_ARGS__)

#define LOG_INFO(...)\
    spdlog::log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::info, __VA_ARGS__)

#define LOG_WARN(...)\
    spdlog::log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::warn, __VA_ARGS__)

#define LOG_ERROR(...)\
    spdlog::log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::err, __VA_ARGS__)

#define LOG_CRITICAL(...)\
    spdlog::log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::critical, __VA_ARGS__)

#define LOG(level, ...)\
    spdlog::log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, level, __VA_ARGS__)

#define LOG_HEX(level, message)\
    LOG(level, "{}", spdlog::to_hex(message.begin(), message.end()));
