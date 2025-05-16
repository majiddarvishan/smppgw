#include "deliver_sm.h"

#include "src/logging/sgw_logger.h"
#include "src/smpp/sgw_external_client.h"

void deliver_sm::process_req(
    int64_t seq,
    std::shared_ptr<SMSC::Protobuf::SMPP::Deliver_Sm_Req> proto_delivery_sm_req,
    const std::string& peer_id,
    std::shared_ptr<smpp_gateway> smpp_gateway)
{
    LOG_DEBUG("process received deliver_sm packet ...");

    if(!smpp_gateway->is_run())
    {
        LOG_ERROR("SMPPGateway already is not running properly");
        return;
    }

    auto deliver_sm_info = std::make_shared<deliver_info>();
    deliver_sm_info->request = proto_delivery_sm_req;
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    deliver_sm_info->deliver_req_received_time_ = microseconds;
    deliver_sm_info->originating_sequence_number_ = seq;
    deliver_sm_info->source_connection_ = peer_id;
    deliver_sm_info->smsc_unique_id_ = proto_delivery_sm_req->smsc_unique_id();

    auto sa = pa::smpp::convert_to_international((pa::smpp::ton)proto_delivery_sm_req->mutable_smpp()->source_addr_ton(), proto_delivery_sm_req->mutable_smpp()->source_addr());
    auto da = pa::smpp::convert_to_international((pa::smpp::ton)proto_delivery_sm_req->mutable_smpp()->dest_addr_ton(), proto_delivery_sm_req->mutable_smpp()->dest_addr());

    sgw_logger::getInstance()->trace_message(
        SMSC::Protobuf::AT_REQ_TYPE,
        proto_delivery_sm_req->smsc_unique_id(),
        "",    /* msg_id */
        SMSC::Trace::Protobuf::ReceiveDeliver,
        peer_id,
        "",
        sa,
        da,
        0, /* error */
        "success");

    smpp_gateway->send_deliver(deliver_sm_info);
}

bool deliver_sm::process_resp(
    std::shared_ptr<smpp_gateway> smpp_gateway,
    std::shared_ptr<deliver_info> user_data_info,
    pa::smpp::command_status      error)
{
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    user_data_info->deliver_resp_sent_time_ = microseconds;
    user_data_info->error_ = error;

    //todo: will be implement in better manner
    if(!user_data_info->is_report_)
    {
        auto sa = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(user_data_info->request->smpp().source_addr_ton()), user_data_info->request->smpp().source_addr());
        auto da = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(user_data_info->request->smpp().dest_addr_ton()), user_data_info->request->smpp().dest_addr());

        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::AT_RESP_TYPE,
            user_data_info->smsc_unique_id_,
            "",            //message_id_,
            SMSC::Trace::Protobuf::ReceiveDeliverResp,
            user_data_info->source_connection_,
            user_data_info->dest_connection_,
            sa,
            da,
            static_cast<int>(error),
            "");

        sgw_logger::getInstance()->log_at(user_data_info);

        if(smpp_gateway->send_to_boninet(SMSC::Protobuf::AT_RESP_TYPE, user_data_info))
        {
            sgw_logger::getInstance()->trace_message(
                SMSC::Protobuf::AT_RESP_TYPE,
                user_data_info->smsc_unique_id_,
                "",
                SMSC::Trace::Protobuf::SendDeliverResp,
                user_data_info->dest_connection_,
                user_data_info->source_connection_,
                sa,
                da,
                0,
                "send successful");

            return true;
        }
        else
        {
            sgw_logger::getInstance()->trace_message(
                SMSC::Protobuf::AT_RESP_TYPE,
                user_data_info->smsc_unique_id_,
                "", //message_id_,
                SMSC::Trace::Protobuf::SendFailed,
                //sys_id,
                user_data_info->source_connection_,
                user_data_info->dest_connection_,
                sa,
                da,
                (int)pa::smpp::command_status::rsyserr,
                "send failed");

            return false;
        }
    }
    else 
    {
        auto sa = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(user_data_info->dr_request->smpp().source_addr_ton()), user_data_info->dr_request->smpp().source_addr());
        auto da = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(user_data_info->dr_request->smpp().dest_addr_ton()), user_data_info->dr_request->smpp().dest_addr());

        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::DR_RESP_TYPE,
            user_data_info->smsc_unique_id_,
            "",            //message_id_,
            SMSC::Trace::Protobuf::ReceiveDeliverResp,
            user_data_info->source_connection_,
            user_data_info->dest_connection_,
            sa,
            da,
            static_cast<int>(error),
            "");

        sgw_logger::getInstance()->log_dr(user_data_info);

        if(smpp_gateway->send_to_boninet(SMSC::Protobuf::DR_RESP_TYPE, user_data_info))
        {
            sgw_logger::getInstance()->trace_message(
                SMSC::Protobuf::DR_RESP_TYPE,
                user_data_info->smsc_unique_id_,
                "",
                SMSC::Trace::Protobuf::SendDeliverResp,
                user_data_info->dest_connection_,
                user_data_info->source_connection_,
                sa,
                da,
                0,
                "send successful");

            return true;
        }
        else
        {
            sgw_logger::getInstance()->trace_message(
                SMSC::Protobuf::DR_RESP_TYPE,
                user_data_info->smsc_unique_id_,
                "", //message_id_,
                SMSC::Trace::Protobuf::SendFailed,
                //sys_id,
                user_data_info->source_connection_,
                user_data_info->dest_connection_,
                sa,
                da,
                (int)pa::smpp::command_status::rsyserr,
                "send failed");

            return false;
        }
    }   
}
