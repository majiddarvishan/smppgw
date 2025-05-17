#include "src/smpp/sgw_external_client.h"
#include "src/paper/paper_client.h"
#include "src/logging/sgw_logger.h"

#include <smpp/utility/unicode_converter.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

void submit_sm::on_check_policies_responce(
    std::shared_ptr<smpp_gateway> smpp_gateway,
    std::shared_ptr<submit_info>  user_data,
    bool                          get_from_paper)
{
    //mshadowQ:todo: should be handle in proper location and delete from here
    if(!smpp_gateway->is_run())
    {
        LOG_ERROR("SMPPGateway already is not running properly");
        return;
    }

    if(get_from_paper)
    {
        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::AO_REQ_TYPE,
            user_data->smsc_unique_id_,
            "",    /*msg_id*/
            SMSC::Trace::Protobuf::PolicyResponse,
            user_data->originating_ext_client_->get_system_id(),
            "",    /*dest_client_id*/
            user_data->international_source_address_,
            user_data->international_dest_address_,
            static_cast<int>(user_data->error_),
            "");
    }

    if(user_data->error_ == pa::smpp::command_status::rok)
    {
        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::AO_REQ_TYPE,
            user_data->smsc_unique_id_,
            "",        /*msg_id*/
            SMSC::Trace::Protobuf::SendSubmit,
            user_data->originating_ext_client_->get_system_id(),
            "",        /*destination_client_id*/
            user_data->international_source_address_,
            user_data->international_dest_address_,
            0,
            "success");

        if(smpp_gateway->send_to_boninet(SMSC::Protobuf::AO_REQ_TYPE, user_data))
        {
            LOG_DEBUG("submit_sm request passed to boninet successfully.");
            return;
        }

        user_data->error_ = pa::smpp::command_status::rsyserr;

        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::AO_REQ_TYPE,
            user_data->smsc_unique_id_,
            "",            /*msg_id*/
            SMSC::Trace::Protobuf::SendFailed,
            user_data->originating_ext_client_->get_system_id(),
            "",            /*destination_client_id*/
            user_data->international_source_address_,
            user_data->international_dest_address_,
            (int)user_data->error_,
            "");
    }

    submit_sm::send_resp(user_data);
    sgw_logger::getInstance()->log_ao_rejected(user_data);
}

void submit_sm::process_resp(
    const std::string                      client_id,
    std::shared_ptr<submit_info>           user_data,
    SMSC::Protobuf::SMPP::Submit_Sm_Resp&& response)
{
    LOG_DEBUG("process received submit_resp(AO_RESP)");
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    user_data->submit_resp_sent_time_ = microseconds;
    user_data->message_id_ = response.md_message_id();
    user_data->error_ = static_cast<pa::smpp::command_status>(response.error_code());

    sgw_logger::getInstance()->trace_message(
        SMSC::Protobuf::AO_RESP_TYPE,
        user_data->smsc_unique_id_,
        user_data->message_id_,
        SMSC::Trace::Protobuf::ReceiveSubmitResp,
        client_id,
        user_data->originating_ext_client_->get_system_id(),
        user_data->international_source_address_,
        user_data->international_dest_address_,
        static_cast<int>(user_data->error_),
        "");

    submit_sm::send_resp(user_data);
}

void submit_sm::send_resp(std::shared_ptr<submit_info> user_data)
{
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    user_data->submit_resp_sent_time_ = microseconds;
    sgw_logger::getInstance()->log_ao(user_data);

    sgw_logger::getInstance()->trace_message(
        SMSC::Protobuf::AO_REQ_TYPE,
        user_data->smsc_unique_id_,
        user_data->message_id_,    /*msg_id*/
        SMSC::Trace::Protobuf::SendSubmitResp,
        user_data->originating_ext_client_->get_system_id(),
        user_data->dest_connection_,
        user_data->international_source_address_,
        user_data->international_dest_address_,
        static_cast<int>(user_data->error_),
        //user_data->error_.GetErrorString());
        "");

    user_data->originating_ext_client_->send_submit_resp(user_data);
}

bool submit_sm::encode_to_protobuf(
    std::shared_ptr<submit_info> user_data,
    std::string&                 encoded)
{
    SMSC::Protobuf::SMPP::Submit_Sm_Req submit;

    submit.set_smsc_unique_id(user_data->smsc_unique_id_);

    submit.set_cp_system_id(user_data->originating_ext_client_->get_system_id());

    submit.mutable_smpp()->set_service_type(user_data->request.service_type);
    // mshadowQ: unused
    //user_data->request.source_addr_ton;
    submit.mutable_smpp()->set_source_addr_ton((uint32_t)user_data->request.source_addr_ton);
    submit.mutable_smpp()->set_source_addr_npi((uint32_t)user_data->request.source_addr_npi);


    //submit.mutable_smpp()->set_source_addr(mSourceAddr.GetValue());
    submit.mutable_smpp()->set_source_addr(user_data->request.source_addr);

    submit.mutable_smpp()->set_dest_addr_ton((uint32_t)user_data->request.dest_addr_ton);
    submit.mutable_smpp()->set_dest_addr_npi((uint32_t)user_data->request.dest_addr_npi);
    submit.mutable_smpp()->set_dest_addr(user_data->request.dest_addr);
    submit.mutable_smpp()->set_esm_class((uint8_t)user_data->request.esm_class);
    submit.mutable_smpp()->set_protocol_id(user_data->request.protocol_id);
    submit.mutable_smpp()->set_priority_flag((uint32_t)user_data->request.priority_flag);

    std::stringstream mystr(user_data->request.schedule_delivery_time);
    int32_t IntSdt = 0;
    mystr >> IntSdt;
    submit.mutable_smpp()->set_schedule_delivery_time(IntSdt);
    mystr.flush();

    mystr << user_data->request.validity_period;
    mystr >> IntSdt;
    submit.mutable_smpp()->set_validity_period(IntSdt);
    mystr.flush();

    //submit.mutable_smpp()->set_schedule_delivery_time((int32_t)stol(pdu->request.schedule_delivery_time));
    //submit.mutable_smpp()->set_validity_period((int32_t)stol(pdu->request.validity_period));

    //submit.mutable_smpp()->set_schedule_delivery_time(10); //TODO
    //submit.mutable_smpp()->set_validity_period(10); //TODO

    submit.mutable_smpp()->set_registered_delivery(uint8_t(user_data->request.registered_delivery));


    submit.mutable_smpp()->set_replace_if_present_flag((int32_t)user_data->request.replace_if_present_flag);
    submit.mutable_smpp()->set_sm_default_msg_id(user_data->request.sm_default_msg_id);

    submit.mutable_body()->set_more_messages_to_send(false);     //TODO
    submit.mutable_body()->set_data_coding((uint32_t)user_data->request.data_coding);
    submit.mutable_body()->set_data_coding_type((::SMSC::Protobuf::DataCodingType)user_data->data_coding_type_);

    submit.mutable_body()->set_sar_msg_ref_num(user_data->concat_ref_num_);
    submit.mutable_body()->set_sar_total_segments(user_data->number_of_parts_);
    submit.mutable_body()->set_sar_segment_seqnum(user_data->part_number_);
    submit.mutable_body()->set_header(user_data->header);
    submit.mutable_body()->set_short_message(user_data->body);

    submit.set_ignore_user_validity_period(user_data->originating_ext_client_->get_ignore_user_validity_period());
    submit.set_max_submit_validity_period(user_data->originating_ext_client_->get_max_submit_validity_period());
    submit.set_max_delivery_report_validity_period(user_data->originating_ext_client_->get_max_delivery_report_validity_period());
    submit.set_submit_resp_message_id_type((SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE)user_data->originating_ext_client_->get_submit_resp_msg_id_base());
    submit.set_delivery_report_message_id_type((SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE)user_data->originating_ext_client_->get_delivery_report_msg_id_base());

    // LogProtobufMessage(spdlog::level::debug, submit);
    log_protobuf_message(submit);

    return submit.SerializeToString(&encoded);
}
