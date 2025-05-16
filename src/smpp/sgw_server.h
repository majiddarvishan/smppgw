#pragma once

#include "src/smpp/sgw_external_client.h"

class routing_matcher;

class sgw_server : public std::enable_shared_from_this<sgw_server>
{
public:
    sgw_server(
        std::shared_ptr<smpp_gateway>            smpp_gateway,
        boost::asio::io_context*                 io_context,
        pa::config::manager*                     config_manager,
        const std::shared_ptr<pa::config::node>& config,
        const std::shared_ptr<pa::config::node>& prometheus_config,
        std::shared_ptr<prometheus::Registry> registry
        );

    virtual ~sgw_server();

    void send_deliver(std::shared_ptr<deliver_info> deliver_info);
    void send_delivery_report(const std::string& orig_cp_id, std::shared_ptr<deliver_info> deliver_info);

private:
    std::string find_route(const std::string& from_id, const std::string& src_address, const std::string& dest_address, packet_type pdu_type);

    /**
     * @brief Retrieves a shared pointer to an external client object by its system ID.
     *
     * This function searches the SGW server's internal map of external clients to find the one with the matching system ID.
     *
     * @param system_id The system ID of the external client to be retrieved.
     *
     * @return Shared pointer to the `sgw_external_client` object if found, otherwise nullptr.
     */

    std::shared_ptr<sgw_external_client> get_external_client(const std::string& system_id);

    //mshadow: check it
    bool is_available(const std::string& id);

    pa::config::manager* config_manager_; /**< Pointer to the configuration manager used for loading and managing sgw_server configuration. */
    std::shared_ptr<smpp_gateway> smpp_gateway_; /**< Shared pointer to the SMPP gateway object used for SMPP communication. */
    boost::asio::io_context* io_context_; /**< Pointer to the io_context used for asynchronous operations. */
    std::shared_ptr<pa::smpp::server> smpp_server_; /**< Shared pointer to the SMPP server instance used for listening on the SMPP port. */

    pa::config::manager::observer config_obs_external_client_insert_;               /**< Observers for external client insertion events. */
    pa::config::manager::observer config_obs_external_client_remove_;               /**< Observers for external client removal events. */
    std::map<std::string, std::shared_ptr<sgw_external_client> > ext_clients_map_;   /**< A map of smpp external client, indexed by their "system-id"s */

    //Server configuration parameters
    std::string ip_;
    int port_;
    std::string system_id_;
    int timeout_;
    uint32_t inactivity_threshold_;
    uint32_t enquirelink_threshold_;
    //

    std::shared_ptr<pa::config::node> prometheus_config_; /**< Shared pointer to the Prometheus configuration node for metric collection. */
    std::shared_ptr<prometheus::Registry> registry_;

    /**
     * @brief Handles an authentication request from an external client.
     *
     * @param bind_request The BIND_REQUEST PDU containing authentication details.
     * @param ip_address The IP address of the SMSC making the request.
     *
     * @return The authentication status as an `pa::smpp::command_status` enum value.
     *         - `ESME_ROK`: Successful authentication.
     *         - `ESME_RINVSERID`: Invalid system ID.
     *         (Other possible errors depending on the implementation of `check_permision`)
     */
    pa::smpp::command_status on_authenticate_request(const pa::smpp::bind_request& bind_request, const std::string& ip_address);

    /**
     * @brief Handles a received BIND request from an pinex and external client.
     *
     * It performs the following tasks:
     *  1. Retrieves the `sgw_external_client` object associated with the system ID from the BIND_REQUEST.
     *  2. If a valid client is found, sets the established session for the client and assigns request/response handlers.
     *      - `on_session_request`: Handles incoming requests.
     *      - `on_session_response`: Handles incoming responses.
     *      - `on_session_send_buf_available`: Informs the client when the session buffer is available for sending data.
     *      - `on_session_close`: Handles session closure events.
     *      - `on_session_deserialization_error`: Handles errors during PDU deserialization.
     *  3. If no valid client is found, logs an error message.
     *
     * @param bind_request The BIND_REQUEST PDU containing details about the SMSC.
     * @param session The shared pointer to the established SMPP session with the SMSC.
     */
    void on_bind(const pa::smpp::bind_request& bind_request, std::shared_ptr<pa::smpp::session> session);

    /**
     * @brief Handles the insertion of a new external client configuration at runtime.
     *
     * This function is called by the SGW server when a new external client configuration is received, potentially during runtime.
     * It performs the following tasks:
     *  1. Extracts the system ID from the configuration.
     *  2. Searches for an existing client with the same system ID in the `ext_clients_map`.
     *  3. If a client with the same ID already exists, logs an informative message and avoids insertion.
     *  4. If no existing client is found, creates a new `sgw_external_client` object using the provided configuration.
     *  5. Inserts the newly created client object into the `ext_clients_map` using the system ID as the key.
     *  6. Logs a message indicating successful client (potentially for removal).
     * @param config Shared pointer to a `pa::config::node` containing the external client configuration details.
     */
    void on_external_client_insert(const std::shared_ptr<pa::config::node>& config);

    /**
     * @brief Handles the removal of an external client configuration at runtime.
     *
     * This function is called by the SGW server when a configuration update removes an external client.
     * It performs the following tasks:
     *  1. Extracts the system ID from the configuration.
     *  2. Searches for the client with the corresponding system ID in the `ext_clients_map`.
     *  3. If a client with the system ID is found, it's removed from the map.
     *  4. If the client is successfully removed, the function returns true.
     *  5. If no client is found with the specified system ID, the function returns false.
     *
     * @param config Shared pointer to a `pa::config::node` containing the external client configuration details.
     */
    void on_external_client_remove(const std::shared_ptr<pa::config::node>& config);

    std::shared_ptr<routing_matcher> routing_;

    // monitoring family
    prometheus::Family<prometheus::Counter>& deliver_routing_family_counter_;
    prometheus::Family<prometheus::Counter>& dr_routing_family_counter_;
    prometheus::Family<prometheus::Counter>& smpp_server_bind_sysid_failed_;
    prometheus::Counter& deliver_routing_failed_;
    prometheus::Counter& dr_routing_failed_;
    prometheus::Counter& connection_reqs_sysid_failed_;
};
