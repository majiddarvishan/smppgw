#pragma once

#include "src/smpp_gateway.h"
#include "src/libs/flow_control.hpp"
#include "src/libs/expirator.hpp"

#include <optional>

class sgw_external_client : public std::enable_shared_from_this<sgw_external_client>
{
public:
    sgw_external_client(
        std::shared_ptr<smpp_gateway>            smpp_gateway,
        boost::asio::io_context*                 io_context,
        pa::config::manager*                     config_manager,
        const std::shared_ptr<pa::config::node>& config,
        const std::shared_ptr<pa::config::node>& prometheus_config,
        std::shared_ptr<prometheus::Registry> registry
        );

    virtual ~sgw_external_client();

    /** @brief close connection and stop the client.
     */
    void stop();

    /** @brief verifies the permissions of an external client to authorized SGW access.
     *
     * @param[in] system_type the system type of the external client.
     * @param[in] password the password provided by the external client.
     * @param[in] ip the iP address of the external client.
     * @param[in] bind_type the bind type requested by the external client.
     *
     * @return pa::smpp::command_status::rok` if permission is granted. else other error command_status
     */
    pa::smpp::command_status check_permision(const std::string&  system_type,
                                             const std::string&  password,
                                             const std::string&  ip,
                                             pa::smpp::bind_type bind_type);

    /** @brief handles session closure for an external client.
     *
     * @param session the closed session.
     * @param error an optional string containing an error message if the session was closed due to an error.
     */
    void on_session_close(std::shared_ptr<pa::smpp::session> session, std::optional<std::string> error);

    /**
     * @brief Handles errors that occur during session deserialization.
     *
     * This function is called when an error is encountered while deserializing a session.
     *
     * @param[in] error The error message that occurred during deserialization.
     * @param command_id The SMPP command ID associated with the session.
     * @param body A span of bytes representing the raw body data of the session.
     */
    void on_session_deserialization_error(std::shared_ptr<pa::smpp::session> session, const std::string& error, pa::smpp::command_id command_id, std::span<const uint8_t> body);

    /**
     * @brief is Called when the session's send buffer becomes available again and logs this event
     *
     */
    void on_session_send_buf_available(std::shared_ptr<pa::smpp::session> session);

    /**
     * @brief Handles incoming SMPP requests from the external client.
     *
     * @details This function is invoked when a new SMPP request arrives on the associated session.
     *
     * @param[in] request The incoming SMPP request object. It's passed by value (`&&`) to avoid unnecessary copies.
     * @param sequence_number The sequence number associated with the request.
     */
    void on_session_request(std::shared_ptr<pa::smpp::session> session, pa::smpp::request&& request, uint32_t sequence_number);

    /**
     * @brief Handles incoming SMPP responses from the SMPP gateway.
     *
     * This function is invoked when a new SMPP response arrives on the associated session.
     *
     * The function retrieves the corresponding `deliver_info`
     * object from the `user_data_` map using the `sequence_number`. If found, it
     * updates the `error_` member of `deliver_info` with the `command_status` from
     * the response. The response is then processed by either
     * `delivery_report_::process_resp` (for delivery reports) or
     * `deliver_sm::process_resp` (for deliveries), depending on the `is_report_` flag
     *  in `deliver_info`.
     *
     * @param response_packet The incoming SMPP response object. It's passed by value (`&&`)
     *        to avoid unnecessary copies.
     * @param sequence_number The sequence number associated with the response, used to
     *        lookup the corresponding `deliver_info` object.
     * @param command_status The SMPP command status code in the response.
     */
    void on_session_response(std::shared_ptr<pa::smpp::session> session, pa::smpp::response&& response, uint32_t sequence_number, pa::smpp::command_status command_status);

    /**
     * @brief Sets the associated SMPP session.
     * @param session A shared pointer to a `pa::smpp::session` object representing
     *        the SMPP session to be associated with this client.
     */
    void set_session(std::shared_ptr<pa::smpp::session> session);

    /** getter */
    SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE get_submit_resp_msg_id_base() const;
    SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE get_delivery_report_msg_id_base() const;
    bool get_ignore_user_validity_period() const;
    int get_max_submit_validity_period() const;
    int get_max_delivery_report_validity_period() const;

    std::string get_system_id();
    std::string get_system_type();

    /** getter */

    /**
     * @brief Sends a submit_resp PDU to the external client.
     * @param user_data A shared pointer to a `submit_info` object containing relevant data
     *                  for constructing and sending the Submit Response PDU.
     *
     * @return `true` if the PDU was sent successfully, `false` otherwise.
     */
    bool send_submit_resp(std::shared_ptr<submit_info> user_data);

    /**
     * @brief flow control before sending deliver_sm PDU to the external client.
     *
     * @param deliver_info Shared pointer to a `deliver_info` object containing message details.
     *
     * @return 0 on success, negative value on error. The specific error code depends on the underlying communication layer.
     */
    void flow_controlled_send_deliver(std::shared_ptr<deliver_info> deliverInfo);
    /**
     * @brief Sends a deliver_sm PDU to the external client.
     *
     * @param deliver_info Shared pointer to a `deliver_info` object containing message details.
     *
     * @return 0 on success, negative value on error. The specific error code depends on the underlying communication layer.
     */
    void send_deliver_sm(std::shared_ptr<deliver_info> deliverInfo);

private:
    void on_packet_expire(uint64_t, std::shared_ptr<deliver_info> user_data);

    void process_deliver_resp(std::shared_ptr<deliver_info> orig_deliver_info);

    void send_process();

    void set_submit_resp_msg_id_base(const std::shared_ptr<pa::config::node>& config);
    void set_delivery_report_msg_id_base(const std::shared_ptr<pa::config::node>& config);

    void set_system_type(const std::shared_ptr<pa::config::node>& config);
    void set_require_password_checking(const std::shared_ptr<pa::config::node>& config);
    void set_require_ip_checking(const std::shared_ptr<pa::config::node>& config);
    void set_ip_mask(const std::shared_ptr<pa::config::node>& config);
    void set_ignore_user_validity_period(const std::shared_ptr<pa::config::node>& config);
    void set_submit_validity_period(const std::shared_ptr<pa::config::node>& config);
    void set_delivery_report_validity_period(const std::shared_ptr<pa::config::node>& config);
    void set_source_address_check(const std::shared_ptr<pa::config::node>& config);
    void set_source_ton_npi_check(const std::shared_ptr<pa::config::node>& config);
    void set_destination_address_check(const std::shared_ptr<pa::config::node>& config);
    void set_destination_ton_npi_check(const std::shared_ptr<pa::config::node>& config);
    void set_dcs_check(const std::shared_ptr<pa::config::node>& config);
    void set_black_white_check(const std::shared_ptr<pa::config::node>& config);
    void set_max_session(const std::shared_ptr<pa::config::node>& config);
    void set_srr_state_generator(const std::shared_ptr<pa::config::node>& config);
    void set_srr_state(const std::shared_ptr<pa::config::node>& config);
    /**
     * @brief Processes a received SUBMIT_SM request.
     *
     * This function handles a SUBMIT_SM request received from an external client via an SMPP gateway.
     * It parses the request, extracts relevant information, and performs various actions based on the request content and client configuration.
     *
     * @param[in] smpp_gateway Reference to an SMSC gateway object.
     * @param[in, out] ext_client Shared pointer to the external client object that sent the request.
     * @param[in] request Reference to the received SUBMIT_SM PDU containing details of the message.
     * @param[in] sequence_number Sequence number assigned to the request.
     * @param[in] session Pointer to the SMPP session object associated with the request.
     */
    void process_submit_req(
        std::shared_ptr<smpp_gateway>        smpp_gateway,
        std::shared_ptr<sgw_external_client> ext_client,
        pa::smpp::submit_sm&&                request,
        uint32_t                             sequence_number,
        std::shared_ptr<pa::smpp::session>   session
        );

    std::shared_ptr<smpp_gateway> smpp_gateway_;
    pa::config::manager* config_manager_;

    std::string system_id_;
    int max_session_;
    bool srr_state_generator_;
    std::string srr_state_;
    std::string system_type_;
    std::string password_;
    bool require_password_checking_;
    bool require_ip_checking_;
    std::string ip_mask_;
    std::vector<std::string> ip_addresses_;
    std::vector<pa::smpp::bind_type> permitted_bind_types_;
    SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE submit_resp_msg_id_base_ = SMSC::Protobuf::DEC;
    SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE delivery_report_msg_id_base_ = SMSC::Protobuf::DEC;

    std::chrono::seconds timeout_sec_;

    bool ignore_user_validity_period_;
    int max_submit_validity_period_;
    int max_delivery_report_validity_period_;

    pa::config::manager::observer config_obs_submit_resp_msg_id_base_;
    pa::config::manager::observer config_obs_delivery_report_msg_id_base_;

    pa::config::manager::observer config_obs_system_type_;
    pa::config::manager::observer config_obs_require_password_checking_;
    pa::config::manager::observer config_obs_require_ip_checking_;
    pa::config::manager::observer config_obs_ip_mask_;
    pa::config::manager::observer config_obs_ignore_user_validity_period_;
    pa::config::manager::observer config_obs_submit_validity_period_;
    pa::config::manager::observer config_obs_delivery_report_validity_period_;
    pa::config::manager::observer config_obs_source_address_check_;
    pa::config::manager::observer config_obs_source_ton_npi_check_;
    pa::config::manager::observer config_obs_destination_address_check_;
    pa::config::manager::observer config_obs_destination_ton_npi_check_;
    pa::config::manager::observer config_obs_dcs_check_;
    pa::config::manager::observer config_obs_black_white_check_;

    pa::config::manager::observer config_obs_max_session_;

    pa::config::manager::observer config_obs_srr_state_generator_;
    pa::config::manager::observer config_obs_srr_state_;

    std::atomic<uint64_t> uptime_;

    std::shared_ptr<io::expirator<uint64_t, std::shared_ptr<deliver_info>>> packet_expirator_;

    std::shared_ptr<io::flow_control> receive_flow_control_;
    std::shared_ptr<io::flow_control> send_flow_control_;

    std::set<std::shared_ptr<pa::smpp::session>> binded_sessions_;

    std::set<pa::paper::proto::Request_Type> policy_commands_;

    std::deque<std::shared_ptr<deliver_info>> send_queue_;

    // monitoring family
    prometheus::Family<prometheus::Gauge>& bind_family_gauge_;
    prometheus::Family<prometheus::Counter>& bind_family_counter_;
    prometheus::Family<prometheus::Counter>& submit_family_counter_;
    prometheus::Family<prometheus::Counter>& submit_resp_family_counter_;
    prometheus::Family<prometheus::Counter>& deliver_family_counter_;
    prometheus::Family<prometheus::Counter>& deliver_resp_family_counter_;
    prometheus::Family<prometheus::Counter>& delivery_report_family_counter_;
    prometheus::Family<prometheus::Counter>& delivery_report_resp_family_counter_;
    prometheus::Family<prometheus::Gauge>& container_family_gauge_;

    // bind monitoring variables
    prometheus::Gauge& connected_connections_;
    prometheus::Counter& connection_reqs_failed_;

    // container monitoring variables
    prometheus::Gauge& wait_for_resp_;

    friend class submit_sm;
    // submits monitoring variables
    prometheus::Counter& submits_received_;
    prometheus::Counter& submits_required_dr_;
    prometheus::Counter& submits_encoding_default_alphabet_;
    prometheus::Counter& submits_encoding_8_bit_ascii_;
    prometheus::Counter& submits_encoding_8_bit_data_;
    prometheus::Counter& submits_encoding_16_bit_data_;
    prometheus::Counter& submits_invalid_data_encoding_;
    prometheus::Counter& submits_rejected_;
    prometheus::Counter& submits_rejected_by_flow_control_;
    prometheus::Counter& submits_rejected_by_policy_;

    // submit response monitoring variables
    prometheus::Counter& submit_resp_status_ok_;
    prometheus::Counter& submit_resp_status_timeout_;
    prometheus::Counter& submit_resp_status_invalid_data_encoding_;
    prometheus::Counter& submit_resp_status_invalid_source_address_;
    prometheus::Counter& submit_resp_status_invalid_source_address_ton_;
    prometheus::Counter& submit_resp_status_invalid_source_address_npi_;
    prometheus::Counter& submit_resp_status_invalid_destination_address_;
    prometheus::Counter& submit_resp_status_invalid_destination_address_ton_;
    prometheus::Counter& submit_resp_status_invalid_destination_address_npi_;
    prometheus::Counter& submit_resp_status_others_error_;

    prometheus::Counter& send_submit_resp_successful_;
    prometheus::Counter& send_submit_resp_failed_;

    // deliver monitoring variables
    // prometheus::Counter& deliver_route_failed_;

    prometheus::Counter& send_deliver_successful_;
    prometheus::Counter& send_deliver_failed_;

    // deliver response monitoring variables
    prometheus::Counter& received_deliver_resp_;
    prometheus::Counter& rejected_deliver_resp_;
    prometheus::Counter& deliver_resp_status_timeout_;
    prometheus::Counter& deliver_resp_status_success_;
    prometheus::Counter& deliver_resp_status_fail_;
    prometheus::Counter& send_deliver_resp_successful_;
    prometheus::Counter& send_deliver_resp_failed_;

    // delivery report monitoring variables
    // prometheus::Counter& dr_route_failed_;

    prometheus::Counter& received_dr_;
    prometheus::Counter& dr_status_delivered_;
    prometheus::Counter& dr_status_expired_;
    prometheus::Counter& dr_status_deleted_;
    prometheus::Counter& dr_status_undeliverable_;
    prometheus::Counter& dr_status_accepted_;
    prometheus::Counter& dr_status_unknown_;
    prometheus::Counter& dr_status_rejected_;
    prometheus::Counter& send_dr_successful_;
    prometheus::Counter& send_dr_failed_;

    // delivery report response monitoring variables
    prometheus::Counter& received_dr_resp_;
    prometheus::Counter& rejected_dr_resp_;
    prometheus::Counter& dr_resp_status_timeout_;
    prometheus::Counter& dr_resp_status_successful_;
    prometheus::Counter& dr_resp_status_failed_;
    prometheus::Counter& send_dr_resp_successful_;
    prometheus::Counter& send_dr_resp_failed_;

    //mshadowQ: not used. check it later
    // prometheus::Counter& rejected_by_flow_control_;
};
