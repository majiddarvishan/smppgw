#pragma once

#include "paper/command.pb.h"
#include "packets/SMPP/SubmitSm.pb.h"

#include <smpp/pdu/submit_sm.hpp>
#include <smpp/net/session.hpp>

#include <memory>

// mshadow: todo: its better use include instead of forward declaration
class smpp_gateway;
struct submit_info;
class sgw_external_client;

class submit_sm
{
public:
    /**
     * @brief Processes a received SUBMIT_SM response.
     *
     * This function handles a SUBMIT_SM response received from an SMPP gateway for a previously submitted message.
     * It updates the internal state of the message with the response information and logs the event.
     *
     * @param[in] client_id ID of the client that submitted the original request.
     * @param[in, out] user_data Shared pointer to the `submit_info` object containing message details.
     * @param[in] response Reference to the received SUBMIT_SM response structure (protobuf message).
     */
    static void process_resp(
        const std::string                      client_id,
        std::shared_ptr<submit_info>           user_data,
        SMSC::Protobuf::SMPP::Submit_Sm_Resp&& respons
        );

    /**
     * @brief Sends a SUBMIT_SM response to the originating client.
     *
     * This function prepares and sends a SUBMIT_SM response back to the external client that submitted the original message.
     * The response includes the message ID and any error code received from the SMPP gateway.
     *
     * @param[in, out] user_data Shared pointer to the `submit_info` object containing message details.
     */
    static void send_resp(std::shared_ptr<submit_info> user_data);


    /**
     * @brief Handles the response from the policy check and sends the SUBMIT_SM request to the SMPP gateway.
     *
     * This function is called after the policy check for a SUBMIT_SM request is complete.
     * It checks the response and takes appropriate actions:
     *  - If the policy check passed (user_data->error_ == pa::smpp::command_status::rok), it sends the SUBMIT_SM request to the SMPP gateway.
     *  - If the policy check failed or sending to the gateway failed, it sets the error code and logs the event.
     *  - In all cases, it sends a response back to the originating client.
     *
     * @param[in] smpp_gateway Pointer to the SMPP gateway object.
     * @param[in, out] user_data Shared pointer to the `submit_info` object containing message details.
     * @param[in] get_from_paper Boolean flag Indicates if the function was called from the policy check process.(default value is ture)
     */
    static void on_check_policies_responce(
        std::shared_ptr<smpp_gateway> smpp_gateway,
        std::shared_ptr<submit_info>  user_data,
        bool                          get_from_paper = true
        );

    /**
     * @brief Encodes a submit_info object into a serialized protobuf message.
     *
     * This function takes a `submit_info` object containing details of a SUBMIT_SM request and encodes it into a serialized string representation using Protobuf.
     * The encoded message can then be sent to the SMPP gateway.
     *
     * @param[in] user_data Shared pointer to the `submit_info` object containing message details.
     * @param[out] encoded Reference to a string that will hold the serialized protobuf message.
     *
     * @return True on success, false on failure.
     */
    static bool encode_to_protobuf(
        std::shared_ptr<submit_info> user_data,
        std::string&                 encoded
        );
};
