#include "src/logging/sgw_logger.h"
#include "src/smpp/sgw_external_client.h"
#include "polar/PacketStream.pb.h"

sgw_logger::~sgw_logger()
{
    close();
}

void sgw_logger::close()
{
    if(reject_logger_)
    {
        reject_logger_->close_file();
    }

    if(ao_logger_)
    {
        ao_logger_->close_file();
    }

    if(at_logger_)
    {
        at_logger_->close_file();
    }

    if(dr_logger_)
    {
        dr_logger_->close_file();
    }

    message_tracer_->stop();
} // sgw_logger::close

void sgw_logger::load_config(boost::asio::io_context* io_context, pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config)
{
    io_context_ = io_context;
    config_manager_ = config_manager;
    config_ = config;

    ao_logger_ = std::make_shared<segmented_logger>(config_manager_, config_->at("ao_logger"));
    at_logger_ = std::make_shared<segmented_logger>(config_manager_, config_->at("at_logger"));
    dr_logger_ = std::make_shared<segmented_logger>(config_manager_, config_->at("dr_logger"));
    reject_logger_ = std::make_shared<segmented_logger>(config_manager_, config_->at("reject_logger"));
    message_tracer_ = std::make_shared<message_tracer>(smpp_gateway_, config_manager_, config_->at("tracer"));

    LOG_INFO("sgw_logger configuration applied successfully.");

    ao_logger_->set_header(R"(ReceivedSubmitTime,SentRespTime,SMPPGWMessageId,SMSCMessageId,SourceConnection,DestConnection,SourceIP,DestinationIP,SourceAddress,DestinationAddress,DataCoding,BodyLen,NumberOfParts,PartNumber,ValidityPeriod,SRR,Error,SystemType,PacketType)");
    at_logger_->set_header(R"(ReceivedDeliverTime,SentRespTime,SourceConnection,DestConnection,SourceIP,DestinationIP,SourceAddress,DestinationAddress,DataCoding,BodyLen,NumberOfParts,PartNumber,Error,SystemType,PacketType)");
    dr_logger_->set_header(R"(ReceivedDeliverTime,SentRespTime,SMPPGWMessageId,SMSCMessageId,SourceConnection,DestConnection,SourceIP,DestinationIP,SourceAddress,DestinationAddress,DataCoding,BodyLen,ValidityPeriod,Status,Error,SystemType,PacketType)");
    reject_logger_->set_header(R"(ReceivedSubmitTime,SentRespTime,SMPPGWMessageId,SMSCMessageId,SourceConnection,DestConnection,SourceIP,DestinationIP,SourceAddress,SourceAddressTON,SourceAddressNPI,DestinationAddress,DestinationAddressTON,DestinationAddressNPI,DataCoding,BodyLen,NumberOfParts,PartNumber,ValidityPeriod,SRR,Error,SystemType,PacketType)");
} // sgw_logger::load_config

void sgw_logger::log_ao(std::shared_ptr<submit_info> packet)
{
    if(!ao_logger_->is_enabled())
        return;

    char log_str[2000] = "\0";
    auto data_coding = pa::smpp::extract_unicode(packet->request.data_coding);
    auto src_address = pa::smpp::convert_to_international(packet->request.source_addr_ton, packet->request.source_addr);
    auto dest_address = pa::smpp::convert_to_international(packet->request.dest_addr_ton, packet->request.dest_addr);

    auto[header, body] = pa::smpp::unpack_short_message(packet->request.esm_class, packet->request.data_coding, packet->request.short_message);

    sprintf(log_str, "%llu,%llu,%s,%s,%s,%s,%s,%s,%s,%s,%u,%u,%d,%d,%s,%d,%d,%s,%s",
            packet->submit_req_received_time_,                                            // 1. ReceivedSubmitTime
            packet->submit_resp_sent_time_,                                               // 2. SentRespTime
            packet->message_id_.c_str(),                                                  // 3. SMPPGWMessageId
            packet->smsc_unique_id_.c_str(),                                              // 4. SMSCMessageId
            packet->source_connection_.c_str(),                                           // 5. SourceConnection
            packet->dest_connection_.c_str(),                                             // 6. DestConnection
            packet->source_ip_.c_str(),                                                   // 7. SourceIP
            packet->destination_ip_.c_str(),                                              // 8. DestinationIP
            src_address.c_str(),                                                          // 9. SourceAddress
            dest_address.c_str(),                                                         // 10. DestinationAddress
            (uint8_t)data_coding,                                                         // 11. DataCoding
            (uint32_t)body.length(),                                                      // 12. BodyLen
            header.get_multi_part_data().number_of_parts_,                                // 13. NumberOfParts
            packet->originating_sequence_number_,                                         // 14. PartNumber
            packet->request.validity_period.c_str(),                                      // 15. ValidityPeriod
            (uint8_t)packet->request.registered_delivery.smsc_delivery_receipt,           // 16. SRR
            (uint32_t)packet->error_,                                                     // 17. Error
            packet->system_type_.c_str(),                                                 // 18. SystemType
            "AO");                                                                        // 19. PacketType

    ao_logger_->record(log_str);
} // sgw_logger::log_ao

void sgw_logger::log_ao_rejected(std::shared_ptr<submit_info> packet)
{
    if(!reject_logger_->is_enabled())
        return;

    char log_str[2000] = "\0";
    auto data_coding = pa::smpp::extract_unicode(packet->request.data_coding);
    auto src_address = pa::smpp::convert_to_international(packet->request.source_addr_ton, packet->request.source_addr);
    auto dest_address = pa::smpp::convert_to_international(packet->request.dest_addr_ton, packet->request.dest_addr);

    auto[header, body] = pa::smpp::unpack_short_message(packet->request.esm_class, packet->request.data_coding, packet->request.short_message);

    sprintf(log_str, "%llu,%llu,%s,%s,%s,%s,%s,%s,%s,%d,%d,%s,%d,%d,%u,%u,%d,%d,%s,%d,%d,%s,%s",
            packet->submit_req_received_time_,                                            // 1. ReceivedSubmitTime
            packet->submit_resp_sent_time_,                                               // 2. SentRespTime
            packet->message_id_.c_str(),                                                  // 3. SMPPGWMessageId
            packet->smsc_unique_id_.c_str(),                                              // 4. SMSCMessageId
            packet->source_connection_.c_str(),                                           // 5. SourceConnection
            packet->dest_connection_.c_str(),                                             // 6. DestConnection
            packet->source_ip_.c_str(),                                                   // 7. SourceIP
            packet->destination_ip_.c_str(),                                              // 8. DestinationIP
            src_address.c_str(),                                                          // 9. SourceAddress
            packet->request.source_addr_ton,                                              // 10. SourceAddressTON
            packet->request.source_addr_npi,                                              // 11. SourceAddressNPI
            dest_address.c_str(),                                                         // 12. DestinationAddress
            packet->request.dest_addr_ton,                                                // 13. DestinationAddressTON
            packet->request.dest_addr_npi,                                                // 14. DestinationAddressNPI
            (uint8_t)data_coding,                                                         // 15. DataCoding
            (uint32_t)body.length(),                                                      // 16. BodyLen
            header.get_multi_part_data().number_of_parts_,                                // 17. NumberOfParts
            packet->originating_sequence_number_,                                         // 18. PartNumber
            packet->request.validity_period.c_str(),                                      // 19. ValidityPeriod
            (uint8_t)packet->request.registered_delivery.smsc_delivery_receipt,           // 20. SRR
            (uint32_t)packet->error_,                                                     // 21. Error
            packet->system_type_.c_str(),                                                 // 22. SystemType
            "AO");                                                                        // 23. PacketType

    reject_logger_->record(log_str);
} // sgw_logger::log_ao_rejected

void sgw_logger::log_at(std::shared_ptr<deliver_info> packet)
{
    if(!at_logger_->is_enabled())
        return;

    char log_str[2000] = "\0";
    auto data_coding = pa::smpp::extract_unicode(static_cast<pa::smpp::data_coding>(packet->request->body().data_coding()));
    auto src_address = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(packet->request->smpp().source_addr_ton()), packet->request->smpp().source_addr());
    auto dest_address = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(packet->request->smpp().dest_addr_ton()), packet->request->smpp().dest_addr());
    pa::smpp::esm_class my_esm_class;
    my_esm_class.from_u8((uint8_t)packet->request->smpp().esm_class());
    auto body = packet->request->body().short_message();

    sprintf(log_str, "%llu,%llu,%s,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%s,%s",
            packet->deliver_req_received_time_,                                            // 1. ReceivedDeliverTime
            packet->deliver_resp_sent_time_,                                               // 2. SentRespTime
            packet->source_connection_.c_str(),                                            // 3. SourceConnection
            packet->dest_connection_.c_str(),                                              // 4. DestConnection
            packet->source_ip_.c_str(),                                                    // 5. SourceIP
            packet->destination_ip_.c_str(),                                               // 6. DestinationIP
            src_address.c_str(),                                                           // 7. SourceAddress
            dest_address.c_str(),                                                          // 8. DestinationAddress
            (uint8_t)data_coding,                                                          // 9. DataCoding
            (uint32_t)body.length(),                                                       // 10. BodyLen
            // header.get_multi_part_data().number_of_parts_,
            // packet->originating_sequence_number_,
            packet->request->body().sar_total_segments(),                                  // 11. NumberOfParts
            packet->request->body().sar_segment_seqnum(),                                  // 12. PartNumber
            (uint32_t)packet->error_,                                                      // 13. Error
            packet->system_type_.c_str(),                                                  // 14. SystemType
            "AT");                                                                         // 15. PacketType

    at_logger_->record(log_str);
} // sgw_logger::log_at

void sgw_logger::log_dr(std::shared_ptr<deliver_info> packet)
{
    if(!dr_logger_->is_enabled())
        return;

    char log_str[2000] = "\0";
    auto data_coding = pa::smpp::extract_unicode(static_cast<pa::smpp::data_coding>(packet->dr_request->body().data_coding()));
    auto src_address = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(packet->dr_request->smpp().source_addr_ton()), packet->dr_request->smpp().source_addr());
    auto dest_address = pa::smpp::convert_to_international(static_cast<pa::smpp::ton>(packet->dr_request->smpp().dest_addr_ton()), packet->dr_request->smpp().dest_addr());
    pa::smpp::esm_class my_esm_class;
    my_esm_class.from_u8((uint8_t)packet->dr_request->smpp().esm_class());
    auto body = packet->dr_request->body().short_message();

    sprintf(log_str, "%llu,%llu,%s,%s,%s,%s,%s,%s,%s,%s,%d,%d,%d,%s,%d,%s,%s",
            packet->deliver_req_received_time_,                                            // 1. ReceivedDeliverTime
            packet->deliver_resp_sent_time_,                                               // 2. SentRespTime
            packet->dr_request->md_message_id().c_str(),                                   // 3. SMPPGWMessageId
            packet->smsc_unique_id_.c_str(),                                               // 4. SMSCMessageId
            packet->source_connection_.c_str(),                                            // 5. SourceConnection
            packet->dest_connection_.c_str(),                                              // 6. DestConnection
            packet->source_ip_.c_str(),                                                    // 7. SourceIP
            packet->destination_ip_.c_str(),                                               // 8. DestinationIP
            src_address.c_str(),                                                           // 9. SourceAddress
            dest_address.c_str(),                                                          // 10. DestinationAddress
            (uint8_t)data_coding,                                                          // 11. DataCoding
            (uint32_t)body.length(),                                                       // 12. BodyLen
            packet->dr_request->smpp().validity_period(),                                  // 13. ValidityPeriod
            packet->dr_status_.c_str(),                                                    // 14. Status
            (uint32_t)packet->error_,                                                      // 15. Error
            packet->system_type_.c_str(),                                                  // 16. SystemType
            "DR");                                                                         // 17. PacketType

    dr_logger_->record(log_str);
} // sgw_logger::log_dr

void sgw_logger::trace_message(uint32_t message_type,const std::string& smsc_unique_id,const std::string& msg_id,SMSC::Trace::Protobuf::Event event,
                               const std::string& origin_client_id,const std::string& dest_client_id,const std::string& source_address,
                               const std::string& destination_address,int error,const std::string& error_str)
{
    message_tracer_->trace_message(message_type,smsc_unique_id,msg_id,event,origin_client_id,dest_client_id,source_address,destination_address,error,error_str);
} // sgw_logger::trace_message
