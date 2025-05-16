#pragma once

#include "src/smpp_gateway.h"
#include "tracer/MessageTracer.pb.h"
#include <cppkafka/producer.h>

class message_tracer
{
public:
    // mshadow:mohsen: add name for input parameters
    message_tracer(std::shared_ptr<smpp_gateway>, pa::config::manager*, const std::shared_ptr<pa::config::node>&);
    virtual ~message_tracer();

    //todo mohsen
    //virtual void GetMonitoringVariables(std::vector<MonitoringVariable>& mvl) override;

    void stop();

    void trace_message(uint32_t                     message_type,
                       const std::string&           smsc_unique_id,
                       const std::string&           msg_id,
                       SMSC::Trace::Protobuf::Event event,
                       const std::string&           origin_client_id,
                       const std::string&           dest_client_id,
                       const std::string&           source_address,
                       const std::string&           destination_address,
                       int                          error,
                       const std::string&           error_str);

private:
    void produce(const std::string& data);

private:
    std::shared_ptr<smpp_gateway> smpp_gateway_;
    pa::config::manager* config_manager_;

    void on_config_replace(const std::shared_ptr<pa::config::node>& config);

    bool enabled_ = false;

    std::unique_ptr<cppkafka::Producer> producer_;

    std::string brokers_;

    std::string topic_;
    std::string request_required_acks = "-1";
    std::string queue_buffering_max_messages = "5000";
    std::string batch_num_messages = "1000";
    std::string queue_buffering_max_ms = "100";
    
    pa::config::manager::observer config_obs_replace_;

    //todo mohsen
    //MonitoringVariable mQueueFullEvent;
};
