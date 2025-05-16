#include "src/logging/message_tracer.h"

#include <chrono>

message_tracer::message_tracer(std::shared_ptr<smpp_gateway> smpp_gateway, pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config)
    : smpp_gateway_(smpp_gateway)
    , config_manager_{config_manager}
    , enabled_ {config->at("enabled")->get<bool>()}
    , brokers_ {config->at("brokers")->get<std::string>()}
    , topic_ {config->at("topic")->get<std::string>()}
    , config_obs_replace_{config_manager->on_replace(config, std::bind_front(&message_tracer::on_config_replace, this))}
{
    //Construct the configuration
    cppkafka::Configuration kafka_config = {
        { "metadata.broker.list", this->brokers_ }
    };

    kafka_config.set_delivery_report_callback([&](cppkafka::Producer&, const cppkafka::Message& msg)
    {
        LOG_INFO("delivery_report_callback topic({})", msg.get_topic());
    });

    this->producer_ = std::make_unique<cppkafka::Producer>(kafka_config);

    //todo mohsen
    //mQueueFullEvent.SetMonitoringName("Broker.QueueFullEvent");
}

message_tracer::~message_tracer()
{
    stop();
}

void message_tracer::stop()
{
    this->producer_->flush();
}

void message_tracer::on_config_replace(const std::shared_ptr<pa::config::node>& config)
{
    //Construct the configuration
    cppkafka::Configuration kafka_config = {
        { "metadata.broker.list", this->brokers_ }
    };

    kafka_config.set_delivery_report_callback([&](cppkafka::Producer&, const cppkafka::Message& msg)
    {
        LOG_INFO("delivery_report_callback topic({})", msg.get_topic());
    });

    this->producer_ = std::make_unique<cppkafka::Producer>(kafka_config);

    //todo mohsen
    //mQueueFullEvent.SetMonitoringName("Broker.QueueFullEvent");
}

//void message_tracer::GetMonitoringVariables(std::vector<MonitoringVariable>& mvl)
//{
//mvl.push_back(mQueueFullEvent);
//}

void message_tracer::trace_message(uint32_t                     message_type,
                                   const std::string&           smsc_unique_id,
                                   const std::string&           msg_id,
                                   SMSC::Trace::Protobuf::Event event,
                                   const std::string&           origin_client_id,
                                   const std::string&           dest_client_id,
                                   const std::string&           source_address,
                                   const std::string&           destination_address,
                                   int                          error,
                                   const std::string&           error_str)
{
    if(this->enabled_)
    {
        auto obj = std::make_unique<SMSC::Trace::Protobuf::Packet>();

        auto microseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        obj->set_current_time(microseconds_since_epoch);

        obj->set_component_name(smpp_gateway_->component_id());
        obj->set_msg_type(message_type);
        obj->set_smsc_unique_id(smsc_unique_id);
        obj->set_msg_id(msg_id);
        obj->set_event(event);
        obj->set_origin_client_id(origin_client_id);
        obj->set_dest_client_id(dest_client_id);
        obj->set_src_addr(source_address);
        obj->set_dest_addr(destination_address);
        obj->set_error_no(error);
        obj->set_error_str(error_str);

        std::string data;
        obj->SerializeToString(&data);

        produce(data);
    }
} //message_tracer::trace_message

void message_tracer::produce(const std::string& data)
{
    //LOG_DEBUG("produce to kafka called");

    this->producer_->poll(std::chrono::milliseconds(0));
    int err = RD_KAFKA_RESP_ERR_UNKNOWN;

    do
    {
        try
        {
            this->producer_->produce(cppkafka::MessageBuilder(this->topic_).payload(data));
            err = RD_KAFKA_RESP_ERR_NO_ERROR;
        }
        catch(cppkafka::HandleException& e)
        {
            err = e.get_error().get_error();

            switch(err)
            {
                case RD_KAFKA_RESP_ERR__QUEUE_FULL:
                    //LOG_WARN("Producer Queue is full for topic <<{}>>", this->topic_.c_str());
                    this->producer_->poll(std::chrono::milliseconds(100));
                    //mQueueFullEvent++;
                    continue;

                case RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC:
                    //LOG_ERROR("Kafka Topic {}, does not exists !", this->topic_.c_str());
                    break;

                default:
                    continue;
                    //LOG_ERROR("produce failed to topic {}, error: %s", this->topic_.c_str(), rd_kafka_err2str(rd_kafka_last_error()));
            } //switch
        }
    }
    while(err == RD_KAFKA_RESP_ERR_UNKNOWN);

    //LOG_DEBUG("Produce to kafka returned");
} //message_tracer::Produce
