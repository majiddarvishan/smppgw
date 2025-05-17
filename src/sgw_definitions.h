#pragma once

#include "libs/logging.hpp"

#include "packets/SMPP/DeliverSm.pb.h"
#include "packets/SMPP/DeliveryReport.pb.h"

#include <smpp/smpp.hpp>

#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

#include <memory>

class sgw_external_client;

#if PROTOBUF_VERSION < 4026000
    #define log_protobuf_message(message) \
    do { \
        if(spdlog::get_level() <= SPDLOG_LEVEL_INFO) \
        { \
            std::string content; \
            [[maybe_unused]] auto s = google::protobuf::json::MessageToJsonString( \
                message, \
                &content, \
                google::protobuf::json::PrintOptions{ .add_whitespace = true, \
                                                    .always_print_primitive_fields = true, \
                                                    .preserve_proto_field_names = true \
                }); \
            LOG_DEBUG("<<Dump {} protobuf message>>", message.GetTypeName()); \
            LOG_DEBUG("{}", content); \
        } \
    } while(0)

    #define log_ptr_protobuf_message(message) \
    do { \
        if(spdlog::get_level() <= SPDLOG_LEVEL_INFO) \
        { \
            std::string content; \
            [[maybe_unused]] auto s = google::protobuf::json::MessageToJsonString( \
                *message, \
                &content, \
                google::protobuf::json::PrintOptions{ .add_whitespace = true, \
                                                    .always_print_primitive_fields = true, \
                                                    .preserve_proto_field_names = true \
                }); \
            LOG_DEBUG("<<Dump {} protobuf message>>", message->GetTypeName()); \
            LOG_DEBUG("{}", content); \
        } \
    } while(0)
#else
    #define log_protobuf_message(message) \
    do { \
        if(spdlog::get_level() <= SPDLOG_LEVEL_INFO) \
        { \
            std::string content; \
            [[maybe_unused]] auto s = google::protobuf::json::MessageToJsonString( \
                message, \
                &content, \
                google::protobuf::json::PrintOptions{ .add_whitespace = true, \
                                                    .always_print_fields_with_no_presence = true, \
                                                    .preserve_proto_field_names = true \
                }); \
            LOG_DEBUG("<<Dump {} protobuf message>>", message.GetTypeName()); \
            LOG_DEBUG("{}", content); \
        } \
    } while(0)

    #define log_ptr_protobuf_message(message) \
    do { \
        if(spdlog::get_level() <= SPDLOG_LEVEL_INFO) \
        { \
            std::string content; \
            [[maybe_unused]] auto s = google::protobuf::json::MessageToJsonString( \
                *message, \
                &content, \
                google::protobuf::json::PrintOptions{ .add_whitespace = true, \
                                                    .always_print_fields_with_no_presence = true, \
                                                    .preserve_proto_field_names = true \
                }); \
            LOG_DEBUG("<<Dump {} protobuf message>>", message->GetTypeName()); \
            LOG_DEBUG("{}", content); \
        } \
    } while(0)
#endif

/**
 * Enumeration representing different types of SMPP packets.
 */
enum class packet_type
{
    unknown = 0,    /**< Unknown packet type (default value). */
    ao      = 1,    /**< submit an SMS message. */
    dr      = 2,    /**< delivery-report an SMS message. */
    at      = 3,    /**< deliver an SMS message. */
};

/**
 * @struct submit_info
 * @brief containing information related to a submitted message.
 */
struct submit_info
{
    uint32_t originating_sequence_number_ = 0;                      /**< Sequence number assigned when the message was originated by external client. */

    std::shared_ptr<sgw_external_client> originating_ext_client_;   /**< Shared pointer to the external client that originated the message. */
    std::shared_ptr<pa::smpp::session>   originating_session_;      /**< Shared pointer to the session that originated the message. */

    uint64_t submit_req_received_time_ = 0;                           /**< Time when the submit request was received. */
    uint64_t submit_resp_sent_time_ = 0;                              /**< Time when the submit response was sent. */

    std::string source_connection_;                                  /**< the source connection status.*/
    std::string dest_connection_;                                    /**< the destination connection status. */
    std::string source_ip_;                                          /**< IP address of the message source. */
    std::string destination_ip_;                                     /**< IP address of the message destination. */
    std::string system_type_;                                        /**< Type of the system that originated the message. */

    pa::smpp::command_status error_;                                 /**< SMPP error status of the message submission (from pa::smpp::command_status). */
    std::string message_id_;                                         /**< Unique message identifier provided by the SMSC. */
    std::string smsc_unique_id_;                                     /**< Unique identifier assigned by SMSC. */
    bool is_multi_part_;                                             /**< Flag indicating if the message is multipart. */

    pa::smpp::data_coding_unicode data_coding_type_;                 /**< Data coding type used for the message (from pa::smpp::data_coding_unicode). */
    std::string body;                                                /**< Body content of the SMS message. */
    std::string header;                                              /**< Optional header information for the SMS message. */
    uint16_t concat_ref_num_;                                        /**< The reference number for a concatenated short message */
    uint8_t number_of_parts_;                                        /**< Number of parts in a multipart message. */
    uint8_t part_number_;                                            /**< Part's sequence in a multipart message. */
    std::string international_source_address_;                       /**< International phone number of the message sender. */
    std::string international_dest_address_;                         /**< International phone number of the message recipient. */

    pa::smpp::submit_sm request;                                     /**< A pa::smpp::submit_sm object containing the all information of submitted PDU */
};

/**
 * @struct deliver_info
 * @brief Struct containing information related to a delivered message.
 *        This struct can be used to represent both the initial delivery request and the
 *        subsequent delivery report (if received). The `is_report_` flag indicates which type
 *        of information is stored.
 */
struct deliver_info
{
    uint32_t originating_sequence_number_ = 0;                              /**< Sequence number assigned to the message by the originating SMPP entity. */

    uint64_t deliver_req_received_time_;                                      /**< Time when the deliver request (or delivery report request) was received. */
    uint64_t deliver_resp_sent_time_;                                         /**< Time when the deliver response (or delivery report response) was sent. */

    std::string source_connection_;                                         /**< the source connection status. */
    std::string dest_connection_;                                           /**< the destination connection status.*/
    std::string source_ip_;                                                 /**< IP address of the source SMPP entity. */
    std::string destination_ip_;                                            /**< IP address of the destination SMPP entity. */
    std::string system_type_;                                               /**< Type of the system involved (e.g., SMSC, ESME). */

    pa::smpp::command_status error_;                                        /**< SMPP command status code received in the delivery response (or delivery report response). */
    std::string dr_status_;                                                 /**< Delivery status information extracted from the delivery report. */

    std::string smsc_unique_id_;                                            /**< Unique identifier assigned by the SMSC to the delivery request. */

    bool is_report_ = false;                                                /**< Flag indicating if this struct holds information from a delivery report (true) or a delivery request as default(false). */
    std::shared_ptr<SMSC::Protobuf::SMPP::Deliver_Sm_Req> request; /**< Shared pointer to the original deliver request details. */          //todo
    std::shared_ptr<SMSC::Protobuf::SMPP::DeliveryReport_Req> dr_request; /**< Shared pointer to the corresponding delivery report request details. only populated if `is_report_` is true. */   //todo
};
