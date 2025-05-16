#include "delivery_report.h"

#include "src/logging/sgw_logger.h"

#include <boost/algorithm/string.hpp>

void delivery_report::extract_dr_status(const std::string& original_body, std::string& dr_status)
{
    std::vector<std::string> splited_body_vector;
    boost::split(splited_body_vector, original_body, boost::is_any_of(" :"), boost::token_compress_on);

    for(unsigned i = 0; i < splited_body_vector.size(); i++)
    {
        if(splited_body_vector[i] == "stat")
        {
            dr_status = splited_body_vector[i + 1];
            break;
        }
    }
}

void delivery_report::process_req(
    int64_t seq,
    std::shared_ptr<SMSC::Protobuf::SMPP::DeliveryReport_Req> dr_req,
    const std::string& peer_id,
    std::shared_ptr<smpp_gateway> smpp_gateway)
{
    LOG_DEBUG("process received delivery_report packet ...");

    if(!smpp_gateway->is_run())
    {
        LOG_ERROR("SMPPGateway already is not running properly");
        return;
    }

    auto user_data = std::make_shared<deliver_info>();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    user_data->deliver_req_received_time_ = microseconds;
    user_data->is_report_ = true;
    user_data->dr_request = dr_req;

    user_data->smsc_unique_id_ = dr_req->smsc_unique_id();
    user_data->originating_sequence_number_ = seq;
    user_data->source_connection_ = peer_id;
    user_data->source_ip_ = ""; //todo mshadow: ?

    extract_dr_status(dr_req->mutable_body()->short_message(), user_data->dr_status_);
    // send_delivery_report(smpp_gateway, dr_req->source_client_id(), user_data);

    auto sa = pa::smpp::convert_to_international((pa::smpp::ton)user_data->dr_request->mutable_smpp()->source_addr_ton(), user_data->dr_request->mutable_smpp()->source_addr());
    auto da = pa::smpp::convert_to_international((pa::smpp::ton)user_data->dr_request->mutable_smpp()->dest_addr_ton(), user_data->dr_request->mutable_smpp()->dest_addr());

    //todo: it's called in wrong place
    sgw_logger::getInstance()->trace_message(
        SMSC::Protobuf::DR_REQ_TYPE,
        user_data->smsc_unique_id_,
        "",        /*msg_id*/
        SMSC::Trace::Protobuf::ReceiveDeliveryReport,
        user_data->source_connection_,
        "",
        sa,
        da,
        0 /*error*/,
        "success");

    LOG_DEBUG("send delivery_report packet ...");
    smpp_gateway->send_delivery_report(dr_req->source_client_id(), user_data);
}

//todo: implement this in external_client
// void delivery_report::send_delivery_report(
//     std::shared_ptr<smpp_gateway> smpp_gateway,
//     const std::string& orig_cp_id,
//     std::shared_ptr<deliver_info> user_data_info)
// {

// }

//todo: we van eliminate error

bool delivery_report::process_resp(
    std::shared_ptr<smpp_gateway> smpp_gateway,
    std::shared_ptr<deliver_info> user_data,
    pa::smpp::command_status error)
{
    user_data->error_ = error;

    std::string sa = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(user_data->dr_request->smpp().source_addr_ton()), user_data->dr_request->smpp().source_addr());
    std::string da = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(user_data->dr_request->smpp().dest_addr_ton()), user_data->dr_request->smpp().dest_addr());

    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    user_data->deliver_resp_sent_time_ = microseconds;

    sgw_logger::getInstance()->trace_message(
        SMSC::Protobuf::DR_RESP_TYPE,
        user_data->smsc_unique_id_,
        user_data->dr_request->md_message_id(),
        SMSC::Trace::Protobuf::ReceiveDeliveryReportResp,
        user_data->dest_connection_,
        user_data->source_connection_,
        sa,
        da,
        static_cast<int>(error),
        "");    //TODO:: Error msg

    sgw_logger::getInstance()->log_dr(user_data);

    if(smpp_gateway->send_to_boninet(SMSC::Protobuf::DR_RESP_TYPE, user_data))
    {
        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::DR_RESP_TYPE,
            user_data->smsc_unique_id_,
            user_data->dr_request->md_message_id(),
            SMSC::Trace::Protobuf::SendDeliveryReportResp,
            user_data->dest_connection_,
            user_data->source_connection_,
            sa,
            da,
            static_cast<int>(error),
            ""); //TODO:: Error msg

        return true;
    }
    else
    {
        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::DR_RESP_TYPE,
            user_data->smsc_unique_id_,
            user_data->dr_request->md_message_id(),
            SMSC::Trace::Protobuf::SendFailed,
            //sys_id,
            user_data->dest_connection_,
            user_data->source_connection_,
            sa,
            da,
            (int)pa::smpp::command_status::rsyserr,
            "send failed");

        return false;
    }
}
