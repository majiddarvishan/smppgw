#include "smpp_gateway.h"
#include "schema.hpp"

#include <boost/asio/signal_set.hpp>

#include <filesystem>

int main()
{
    std::filesystem::path config_file_path = std::filesystem::current_path() / "config.json";
    auto config_manager = pa::config::manager{std::make_unique<pa::config::file_source>(config_file_path.string(), schema)};
    const auto config = config_manager.config();

    logging log{ &config_manager, config->at("logging") };

    LOG_CRITICAL("Version: {}.{}.{}", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    LOG_CRITICAL("Git Path: {}", GIT_PATH);
    LOG_CRITICAL("Git Branch: {}", GIT_BRANCH);
    LOG_CRITICAL("Git Revision: {}", GIT_REVISION);

    LOG_CRITICAL("Local changes of source code: {}", LOCAL_CHANGES == 1 ? "Detected!" : "Nothing");
    LOG_CRITICAL("Built on: {}, {} ", SERVER_IP, SERVER_TIME);
    LOG_CRITICAL("Info: {}", SERVER_OS);

    boost::asio::io_context io_context;

    const auto port = config->at("config_server")->at("port")->get<uint16_t>();
    const auto api_key = config->at("config_server")->at("api_key")->get<std::string>();
    auto config_http_server = pa::config::http_server{ &io_context, &config_manager, port, api_key };

    std::shared_ptr<smpp_gateway> smpp_gateway_ = std::make_shared<smpp_gateway>(&io_context, &config_manager, config->at("smpp_gateway"));
    smpp_gateway_->initilize();
    smpp_gateway_->start();

    LOG_CRITICAL("SMPPGateway is running");

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const std::error_code& ec, int) {
        if(!ec)
        {
            smpp_gateway_->stop();
            config_http_server.stop();
            io_context.stop();
        }
    });

    io_context.run();

    LOG_CRITICAL("SMPPGateway is shutting down");
    return 0;
} //main
