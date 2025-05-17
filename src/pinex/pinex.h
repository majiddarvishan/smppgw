#pragma once

#include "src/routing/pinex/router.h"
#include "src/smpp/submit_sm.h"
#include "src/sgw_definitions.h"
#include "src/libs/monitoring.hpp"

#include <pinex/net/definitions.hpp>
#include <pinex/p_client_list.hpp>

#include <prometheus/registry.h>

#include <typeinfo>
#include <unordered_map>

class smpp_gateway;

class pinex : public std::enable_shared_from_this<pinex>
{
    bool encode_to_protobuf(std::shared_ptr<deliver_info> deliver_info, std::string& encoded)
    {
        if(deliver_info->is_report_)
        {
            SMSC::Protobuf::SMPP::DeliveryReport_Resp deliver_report_resp;
            deliver_report_resp.set_error_code((uint32_t)deliver_info->error_);
            deliver_report_resp.set_smsc_unique_id(deliver_info->smsc_unique_id_);
            //deliver_report_resp.set_system_id(systemId);
            deliver_report_resp.SerializeToString(&encoded);
            log_protobuf_message(deliver_report_resp);
            return true;
        }

        SMSC::Protobuf::SMPP::Deliver_Sm_Resp deliver_sm_resp;
        deliver_sm_resp.set_error_code((uint32_t)deliver_info->error_);
        deliver_sm_resp.set_smsc_unique_id(deliver_info->smsc_unique_id_);
        //deliver_sm_resp.set_system_id(systemId);
        deliver_sm_resp.SerializeToString(&encoded);
        log_protobuf_message(deliver_sm_resp);
        return true;
    }

public:
    pinex(
        std::shared_ptr<smpp_gateway>            smpp_gateway,
        boost::asio::io_context*                 io_context,
        pa::config::manager*                     config_manager,
        const std::shared_ptr<pa::config::node>& config,
        const std::shared_ptr<pa::config::node>& prometheus_config,
        std::shared_ptr<prometheus::Registry> registry);

    virtual ~pinex();

    void start();
    void stop();

    template<typename info_type>
    bool send(int msg_type, info_type user_data)
    {
        std::string send_msg(4, '\0');
        *((int*)send_msg.c_str()) = msg_type;

        using datatype = std::decay_t<decltype(*user_data)>;

        if constexpr(std::is_same_v<datatype, submit_info>)
        {
            std::string encoded_pdu;
            submit_sm::encode_to_protobuf(user_data, encoded_pdu);
            send_msg += encoded_pdu;
            LOG_HEX(spdlog::level::debug, send_msg);

            // switch(msg_type)
            // {
            //     case SMSC::Protobuf::AO_REQ_TYPE:
            //         submit_req_sent_.Increment();
            //         break;

            //     case SMSC::Protobuf::AT_REQ_TYPE:
            //     case SMSC::Protobuf::DR_REQ_TYPE:
            //     default:
            //         LOG_DEBUG("This packet type({}) is not valid!", msg_type);
            // }//switch

            std::vector<std::string> destinations;
            router_->find(msg_type, user_data->international_dest_address_, destinations);

            if(!destinations.empty())
            {
                LOG_INFO("Send submit to client {}", destinations[0]);

                try
                {
                    auto seq_no = client_list_->send_request(send_msg, destinations[0]);

                    if(seq_no)
                    {
                        user_data_.insert({ fmt::format("{}@{}", seq_no, destinations[0]), user_data });
                        wait_for_resp_.Increment();
                        switch(msg_type)
                        {
                            case SMSC::Protobuf::AO_REQ_TYPE:
                                submit_req_sending_succeed_.Increment();
                                break;
                            case SMSC::Protobuf::AT_REQ_TYPE:
                            case SMSC::Protobuf::DR_REQ_TYPE:
                            default:
                                LOG_DEBUG("This packet type({}) is not valid!", msg_type);
                        }//switch
                        return true;
                    }
                }
                catch (const std::exception& ex)
                {
                    LOG_ERROR("catch an exception on send, {}", ex.what());
                }
                catch (...)
                {
                    std::exception_ptr p = std::current_exception();
                    LOG_ERROR("catch an exception on send, {}", (p ? p.__cxa_exception_type()->name() : "null"));
                }

                LOG_ERROR("Could not send message {} to message dispatcher", msg_type);

                switch(msg_type)
                {
                    case SMSC::Protobuf::AO_REQ_TYPE:
                        submit_req_sending_failed_.Increment();
                        break;
                    case SMSC::Protobuf::AT_REQ_TYPE:
                    case SMSC::Protobuf::DR_REQ_TYPE:
                    default:
                        LOG_DEBUG("This packet type({}) is not valid!", msg_type);
                }//switch

                return false;
            }

            LOG_ERROR("Could not find route for msg ({}), destination {}", msg_type, user_data->source_connection_.c_str());

            switch(msg_type)
            {
                case SMSC::Protobuf::AO_REQ_TYPE:
                    submit_req_routing_failed_.Increment();
                    break;
                case SMSC::Protobuf::AT_REQ_TYPE:
                case SMSC::Protobuf::DR_REQ_TYPE:
                default:
                    LOG_DEBUG("This packet type({}) is not valid!", msg_type);
            }//switch

            return false;
        }

        if constexpr(std::is_same_v<datatype, deliver_info>)
        {
            // if(msg_type ==  SMSC::Protobuf::DR_RESP_TYPE)
            //     dr_resp_sent_.Increment();
            // else if(msg_type == SMSC::Protobuf::AT_RESP_TYPE)
            //     deliver_resp_sent_.Increment();

            std::string encoded_pdu;
            encode_to_protobuf(user_data, encoded_pdu);
            send_msg += encoded_pdu;
            LOG_HEX(spdlog::level::debug, send_msg);

            try
            {
                client_list_->send_response(send_msg, user_data->originating_sequence_number_, user_data->source_connection_);

                if(msg_type ==  SMSC::Protobuf::DR_RESP_TYPE)
                    dr_resp_sending_succeed_.Increment();
                else if(msg_type == SMSC::Protobuf::AT_RESP_TYPE)
                    deliver_resp_sending_succeed_.Increment();

                return true;
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR("catch an exception on send, {}", ex.what());
            }
            catch (...)
            {
                std::exception_ptr p = std::current_exception();
                LOG_ERROR("catch an exception on send, {}", (p ? p.__cxa_exception_type()->name() : "null"));
            }

            if(msg_type ==  SMSC::Protobuf::DR_RESP_TYPE)
                dr_resp_sending_failed_.Increment();
            else if(msg_type == SMSC::Protobuf::AT_RESP_TYPE)
                deliver_resp_sending_failed_.Increment();

            return false;
        }

        return false;
    }

    inline std::string& client_id()
    {
        return client_name_;
    }

private:
    void receive_request(const std::string& client_id, uint32_t seq_no, const std::string& msg_body);
    void receive_response(const std::string& client_id, uint32_t seq_no, const std::string& msg_body);
    void timeout_request(const std::string& client_id, uint32_t seq_no, const std::string& msg_body);
    void session_state_changed(const std::string& name, session_stat state);

    void update_delivery_report_status(std::string status);

    void on_config_replace(const std::shared_ptr<pa::config::node>&);

    boost::asio::io_context* io_context_;

    std::shared_ptr<smpp_gateway> smpp_gateway_;

    pa::config::manager* config_manager_;
    std::shared_ptr<pa::config::node> config_;
    const std::shared_ptr<pa::config::node> prometheus_config_;

    std::shared_ptr<router> router_;

    std::vector<std::string> trasnports_;
    std::string client_name_;
    uint32_t timeout_;

    pa::config::manager::observer config_obs_replace_;

    std::shared_ptr<pa::pinex::p_client_list> client_list_;

    std::unordered_map<std::string, std::shared_ptr<submit_info> > user_data_;

    prometheus::Family<prometheus::Gauge>& pinex_connection_family_gauge_;
    prometheus::Family<prometheus::Counter>& submit_family_counter_;
    prometheus::Family<prometheus::Counter>& submit_resp_family_counter_;
    prometheus::Family<prometheus::Counter>& deliver_family_counter_;
    prometheus::Family<prometheus::Counter>& deliver_resp_family_counter_;
    prometheus::Family<prometheus::Counter>& delivery_report_family_counter_;
    prometheus::Family<prometheus::Counter>& delivery_report_resp_family_counter_;
    prometheus::Family<prometheus::Gauge>& container_family_gauge_;

    prometheus::Gauge& connected_clients_;
    prometheus::Gauge& wait_for_resp_;

    prometheus::Counter& submit_req_sending_succeed_;
    prometheus::Counter& submit_req_sending_failed_;
    prometheus::Counter& submit_req_routing_failed_;
    prometheus::Counter& submit_resp_received_;
    prometheus::Counter& submit_resp_status_success_;
    prometheus::Counter& submit_resp_status_fail_;
    prometheus::Counter& submit_resp_timeout_;
    prometheus::Counter& deliver_req_received_;
    prometheus::Counter& deliver_resp_sending_succeed_;
    prometheus::Counter& deliver_resp_sending_failed_;
    prometheus::Counter& dr_req_received_;
    prometheus::Counter& dr_resp_sending_succeed_;
    prometheus::Counter& dr_resp_sending_failed_;
    prometheus::Counter& dr_status_delivered_;
    prometheus::Counter& dr_status_expired_;
    prometheus::Counter& dr_status_deleted_;
    prometheus::Counter& dr_status_undeliverable_;
    prometheus::Counter& dr_status_accepted_;
    prometheus::Counter& dr_status_unknown_;
    prometheus::Counter& dr_status_rejected_;
};
