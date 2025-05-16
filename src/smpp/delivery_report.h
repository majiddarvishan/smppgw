#pragma once

#include "src/sgw_definitions.h"

class smpp_gateway;

class delivery_report
{
public:
    /**
     * @brief Processes a Delivery Report request received from MD(Message Dispatcher)
     *
     * It performs the following tasks:
     *  1. store DR information.
     *  2. find the appropriate external client based on routing information to send dr.
     *  3. Notifies the external client about receiving a Delivery Report request (potentially with additional details).
     *  4. send a delivery report response back to the MD.
     *
     * @param seq Sequence number of the received dr request.
     * @param dr_req Shared pointer to the received protobuf Delivery Report request object.
     * @param peer_id Peer ID of the MD connection.
     * @param smpp_gateway Shared pointer to the SMPP gateway object.
     */
    static void process_req(
        int64_t seq,
        std::shared_ptr<SMSC::Protobuf::SMPP::DeliveryReport_Req> dr_req,
        const std::string& peer_id,
        std::shared_ptr<smpp_gateway> smpp_gateway);

    /**
     * @brief Sends a Delivery Report(DR) response back to the MD(Message Dispatcher).
     *
     * It performs the following tasks:
     *  1. Retrieves the `sgw_external_client` object associated with the original client ID.
     *  2. Checks if the client is found and bonded (actively connected).
     *      - If not found or not bonded, logs an error, sends a generic delivery report via route,
     *        and calls `process_resp` to handle the error response.
     *  3. Updates the external client's internal state with the delivery status.
     *  4. Extracts source and destination addresses in international format.
     *  5. Logs trace messages for both sending and receiving the DR using `sgw_logger`.
     *  6. Calls `send_deliver_sm` on the external client to send the DR.
     *      - If successful, notifies the external client about sending the DR.
     *      - If unsuccessful, logs an error, sends a generic delivery report via route,
     *        and calls `process_resp` to handle the error response.
     *
     * @param smpp_gateway Shared pointer to the SMPP gateway object.
     * @param orig_cp_id The original client ID.
     * @param user_data_info Shared pointer to the `deliver_info` object containing DR information.
     */
    // static void send_delivery_report(
    //     std::shared_ptr<smpp_gateway> smpp_gateway,
    //     const std::string& orig_cp_id,
    //     std::shared_ptr<deliver_info> error);

    /**
     * @brief Processes the response to a Delivery Report (DR) request sent to MD (Message Dispatcher).
     *
     * It performs the following tasks:
     *  1. find the appropriate external client based on routing information
     *  2. Retrieves the `sgw_external_client` object associated with the identified client ID.
     *  3. Handles the response based on the error code:
     *      - If timeout: Notifies the external client about a delivery report timeout.
     *      - Otherwise: Notifies the external client about receiving a delivery report response with the corresponding error code.
     *  4. Logs a trace message for receiving the DR response.
     *  5. Logs the delivery report information using `sgw_logger`.
     *  6. Attempts to send the DR response to MD
     *
     * @param smpp_gateway Shared pointer to the SMPP gateway object.
     * @param user_data_info Shared pointer to the `deliver_info` object containing DR information.
     * @param error SMPP command status error code received in the response.
     *
     * @return true, if response has been sent to MD,
     *         false, otherwise
     */
    static bool process_resp(
        std::shared_ptr<smpp_gateway> smpp_gateway,
        std::shared_ptr<deliver_info> user_data_info,
        pa::smpp::command_status error);

    /**
     * @brief Extracts the Delivery Receipt(DR) status from a message body.
     *
     * @param[in] original_body  The original message body string.
     * @param[out] dr_status  Extracted DR status string (empty if not found).
     *
     * @return  void
     */
    static void extract_dr_status(
        const std::string& original_body,
        std::string& dr_status);
};
