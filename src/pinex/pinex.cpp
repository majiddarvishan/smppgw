#include "pinex.h"

#include "src/smpp/deliver_sm.h"
#include "src/smpp/delivery_report.h"
#include "src/smpp_gateway.h"

#include <unistd.h>

pinex::pinex(
    std::shared_ptr<smpp_gateway>            smpp_gateway,
    boost::asio::io_context*                 io_context,
    pa::config::manager*                     config_manager,
    const std::shared_ptr<pa::config::node>& config,
    const std::shared_ptr<pa::config::node>& prometheus_config,
    std::shared_ptr<prometheus::Registry> registry)
    : io_context_ { io_context }
    , smpp_gateway_ { smpp_gateway }
    , config_manager_ { config_manager }
    , config_ { config }
    , prometheus_config_{prometheus_config}
    , router_ { std::make_shared<router>(config_manager_, config_->at("router"), [](const std::string& t) -> int{
        if(t == "submit")
            return static_cast<int>(SMSC::Protobuf::AO_REQ_TYPE);

        if(t == "deliver")
            return static_cast<int>(SMSC::Protobuf::AT_REQ_TYPE);

        if(t == "delivery_report")
            return static_cast<int>(SMSC::Protobuf::DR_REQ_TYPE);

        LOG_ERROR("invalid routing msg_type {}!", t);
        throw std::runtime_error("invalid routing msg_type!");
    }) }
    , client_name_ { config_->at("name")->get<std::string>() }
    , timeout_ { config_->at("timeout")->get<std::uint32_t>() }
    , pinex_connection_family_gauge_{prometheus::BuildGauge().Name("pinex_connection").Help("pinex parameters").Register(*registry)}
    , submit_family_counter_{prometheus::BuildCounter().Name("pinex_submit").Help("pinex submit parameters").Register(*registry)}
    , submit_resp_family_counter_{prometheus::BuildCounter().Name("pinex_submit_resp").Help("pinex submit resp parameters").Register(*registry)}
    , deliver_family_counter_{prometheus::BuildCounter().Name("pinex_deliver").Help("pinex deliver parameters").Register(*registry)}
    , deliver_resp_family_counter_{prometheus::BuildCounter().Name("pinex_deliver_resp").Help("pinex deliver resp parameters").Register(*registry)}
    , delivery_report_family_counter_{prometheus::BuildCounter().Name("pinex_delivery_report").Help("pinex delivery report parameters").Register(*registry)}
    , delivery_report_resp_family_counter_{prometheus::BuildCounter().Name("pinex_delivery_report_resp").Help("pinex delivery report resp parameters").Register(*registry)}
    , container_family_gauge_(prometheus::BuildGauge().Name("pinex_container_size").Help("pinex~ container size").Register(*registry))
    , connected_clients_(add_gauge(pinex_connection_family_gauge_, prometheus_config_->at("labels"), {
    { "name", "connected_clients" }, { "category", "network" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , wait_for_resp_(add_gauge(container_family_gauge_, prometheus_config->at("labels"), {
    { "name", "wait_for_resp" }, { "interface", "pinex" }
}))
    , submit_req_sending_succeed_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_succeed" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , submit_req_sending_failed_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_failed" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , submit_req_routing_failed_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "routing_failed" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , submit_resp_received_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "received" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , submit_resp_status_success_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "success_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , submit_resp_status_fail_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "fail_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , submit_resp_timeout_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "timeout_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , deliver_req_received_(add_counter(deliver_family_counter_, prometheus_config->at("labels"), {
    { "name", "received" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , deliver_resp_sending_succeed_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_succeed" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , deliver_resp_sending_failed_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_failed" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_req_received_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "received" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_resp_sending_succeed_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_succeed" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_resp_sending_failed_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_failed" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_status_delivered_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "delivered_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_status_expired_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "expired_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_status_deleted_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "deleted_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_status_undeliverable_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "undeliverable_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_status_accepted_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "accepted_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_status_unknown_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "unknown_status" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , dr_status_rejected_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "rejected_status" }, { "system_id", config->at("name")->get<std::string>() }
}))

{
    for(auto& n : config_->at("address")->nodes())
        trasnports_.push_back(n->get<std::string>());
}

pinex::~pinex() {}

void pinex::start()
{
    if (!client_list_)
    {
        client_list_ = std::make_shared<pa::pinex::p_client_list>(
          io_context_,
          client_name_,
          trasnports_,
          timeout_,
          true, /*auto_reconnect*/
          std::bind_front(&pinex::receive_request, this),
          std::bind_front(&pinex::receive_response, this),
          std::bind_front(&pinex::timeout_request, this),
          std::bind_front(&pinex::session_state_changed, this)
        );
    }
} // pinex::start

void pinex::stop()
{
    if (client_list_)
    {
        client_list_->stop();
        client_list_ = nullptr;
    }
    sleep(1);
}

void pinex::session_state_changed(const std::string& name, session_stat state)
{
    switch (state)
    {
        case session_stat::open:
            LOG_INFO("State of connection {} => {} is changed to open", client_name_, name);
            connected_clients_.Decrement();
            router_->remove_target(name);
            break;

        case session_stat::bind:
            LOG_INFO("State of connection {} => {} is changed to bind", client_name_, name);
            connected_clients_.Increment();
            router_->add_target(name);
            break;

        case session_stat::close:
            LOG_INFO("State of connection {} => {} is changed to close", client_name_,name);
            connected_clients_.Decrement();
            router_->remove_target(name);
            break;
    } // switch
} // pinex::session_state_changed

void pinex::receive_request(const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    LOG_DEBUG("Received request with sequence: {} from client: {}", seq_no, client_id);

    uint32_t msg_type = (*(uint32_t*) msg_body.substr(0, 4).c_str());

    switch (msg_type)
    {
        case SMSC::Protobuf::AT_REQ_TYPE:
        {
            LOG_DEBUG("Received request msg_type is AT_REQ_TYPE");

            auto proto_deliver_sm_req = std::make_shared<SMSC::Protobuf::SMPP::Deliver_Sm_Req>();
            proto_deliver_sm_req->ParseFromString(msg_body.substr(4));
            log_ptr_protobuf_message(proto_deliver_sm_req);
            deliver_req_received_.Increment();

            LOG_HEX(spdlog::level::info, proto_deliver_sm_req->body().short_message());

            deliver_sm::process_req(static_cast<uint64_t>(seq_no), proto_deliver_sm_req, client_id, smpp_gateway_);
            break;
        }

        case SMSC::Protobuf::DR_REQ_TYPE:
        {
            LOG_DEBUG("Received request msg_type is DR_REQ_TYPE");

            auto proto_delivery_report_req = std::make_shared<SMSC::Protobuf::SMPP::DeliveryReport_Req>();
            proto_delivery_report_req->ParseFromString(msg_body.substr(4));
            log_ptr_protobuf_message(proto_delivery_report_req);
            dr_req_received_.Increment();
            delivery_report::process_req(static_cast<uint64_t>(seq_no), proto_delivery_report_req, client_id, smpp_gateway_);
            std::string dr_status;
            delivery_report::extract_dr_status(proto_delivery_report_req->mutable_body()->short_message(), dr_status);
            update_delivery_report_status(dr_status);
            break;
        }

        default:
        {
            LOG_DEBUG("Received request msg_type is unknown");
            break;
        }
    } // switch
} // pinex::receive_request

void pinex::update_delivery_report_status(std::string status)
{
    std::transform(status.begin(), status.end(), status.begin(), ::toupper);

    if (status.compare("DELIVRD") == 0)
    {
        dr_status_delivered_.Increment();
    }
    else if (status.compare("EXPIRED") == 0)
    {
        dr_status_expired_.Increment();
    }
    else if (status.compare("DELETED") == 0)
    {
        dr_status_deleted_.Increment();
    }
    else if (status.compare("UNDELIV") == 0)
    {
        dr_status_undeliverable_.Increment();
    }
    else if (status.compare("ACCEPTD") == 0)
    {
        dr_status_accepted_.Increment();
    }
    else if (status.compare("UNKNOWN") == 0)
    {
        dr_status_unknown_.Increment();
    }
    else if (status.compare("REJECTD") == 0)
    {
        dr_status_rejected_.Increment();
    }
    else
    {
        LOG_DEBUG("This status({}) is not valid?!", status.c_str());
    }
}
// mohsen

void pinex::receive_response(
  const std::string& client_id, uint32_t seq_no, const std::string& msg_body
)
{
    LOG_DEBUG("Receive response with sequence: {} from client: {}", seq_no, client_id);

    const auto& it = user_data_.find(fmt::format("{}@{}", seq_no, client_id));

    if (it == user_data_.end())
    {
        LOG_ERROR("Could not find user_data for sequence {}", seq_no);
        return;
    }

    auto orig_submit_info = it->second;
    user_data_.erase(it);
    wait_for_resp_.Decrement();

    uint32_t msg_type = (*(uint32_t*) msg_body.substr(0, 4).c_str());

    switch (msg_type)
    {
        case SMSC::Protobuf::AO_RESP_TYPE:
        {
            LOG_DEBUG("Receive AO_RESP_TYPE");

            SMSC::Protobuf::SMPP::Submit_Sm_Resp resp;
            resp.ParseFromString(msg_body.substr(4));
            log_protobuf_message(resp);
            submit_resp_received_.Increment();
            if (resp.error_code())
                submit_resp_status_fail_.Increment();
            else
                submit_resp_status_success_.Increment();

            submit_sm::
              process_resp(client_id, orig_submit_info, std::move(resp));
            break;
        }

        default:
        {
            LOG_DEBUG("default {}", msg_type);
            break;
        }
    } // switch
} // pinex::receive_response

void pinex::timeout_request(const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    LOG_DEBUG("Timeout request with sequence: {} from client: {}", seq_no, client_id);

    const auto& it = user_data_.find(fmt::format("{}@{}", seq_no, client_id));

    if (it == user_data_.end())
    {
        LOG_ERROR("Could not find user_data for sequence {}", seq_no);
        return;
    }

    auto orig_submit_info = it->second;
    user_data_.erase(it);
    wait_for_resp_.Decrement();

    uint32_t msg_type = (*(uint32_t*) msg_body.substr(0, 4).c_str());

    switch (msg_type)
    {
        case SMSC::Protobuf::AO_REQ_TYPE:
        {
            SMSC::Protobuf::SMPP::Submit_Sm_Req req;
            req.ParseFromString(msg_body.substr(4));

            SMSC::Protobuf::SMPP::Submit_Sm_Resp resp;
            resp.set_error_code(
              static_cast<uint32_t>(pa::smpp::command_status::rtimeout)
            );
            resp.set_smsc_unique_id(req.smsc_unique_id());

            log_protobuf_message(resp);
            submit_resp_timeout_.Increment();
            submit_sm::
              process_resp(client_id, orig_submit_info, std::move(resp));

            break;
        }

        default:
        {
            LOG_DEBUG("The request message type({}) is default", msg_type);
            break;
        }
    } // switch
} // pinex::timeout_request