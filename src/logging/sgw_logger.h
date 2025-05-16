#pragma once

#include "src/libs/segmented_logger.hpp"
#include "src/logging/message_tracer.h"

struct submit_info;
struct deliver_info;

class sgw_logger
{
private:
    sgw_logger() = default;
    sgw_logger(sgw_logger const&) = delete;
    void operator=(sgw_logger const&) = delete;

    static sgw_logger* instacne;

public:
    std::shared_ptr<smpp_gateway> smpp_gateway_;

    static std::shared_ptr<sgw_logger> getInstance()
    {
        static std::shared_ptr<sgw_logger> instance(new sgw_logger);
        return instance;
    }

    void SetSmppGateway(std::shared_ptr<smpp_gateway> smpp_gateway)
    {
        this->smpp_gateway_ = smpp_gateway;
    }

    virtual ~sgw_logger();

    void close();

    pa::config::manager* config_manager_;
    std::shared_ptr<pa::config::node> config_ = nullptr;
    boost::asio::io_context* io_context_;
    void load_config(boost::asio::io_context* io_context, pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config);

    void log_ao(std::shared_ptr<submit_info> packet);
    void log_at(std::shared_ptr<deliver_info> packet);
    void log_dr(std::shared_ptr<deliver_info> packet);
    void log_ao_rejected(std::shared_ptr<submit_info> packet);

    void trace_message(uint32_t message_type,const std::string& smsc_unique_id,const std::string& msg_id,SMSC::Trace::Protobuf::Event event,
                       const std::string& origin_client_id,const std::string& dest_client_id,const std::string& source_address,
                       const std::string& destination_address,int error,const std::string& error_str);

private:
    std::shared_ptr<segmented_logger> ao_logger_;
    std::shared_ptr<segmented_logger> at_logger_;
    std::shared_ptr<segmented_logger> dr_logger_;
    std::shared_ptr<segmented_logger> reject_logger_;
    std::shared_ptr<message_tracer> message_tracer_;
};
