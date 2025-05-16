#pragma once

#include "src/sgw_definitions.h"

class smpp_gateway;
//mshadowD: check doxygen again
class deliver_sm
{
public:
    /**
     * @brief Processes a received Protobuf Deliver Request from pinex.
     *
     * It performs the following steps:
     *   - Store request details.
     *   - Finds the appropriate route for the delivery.
     *   - Retrieves the external client associated with the route.
     *   - Validates if the client is bound and ready to receive messages.
     *   - Sends the deliver_sm request to the external client.
     *
     * @param seq The sequence number of the received request.
     * @param proto_delivery_sm_req A shared pointer to request details.
     * @param[in] peer_id The ID of the peer that sent the request.
     * @param smpp_gateway A shared pointer to the `smpp_gateway` object, providing access to smpp_gateway functionalities.
     */
    static void process_req(
        int64_t                                               seq,
        std::shared_ptr<SMSC::Protobuf::SMPP::Deliver_Sm_Req> proto_delivery_sm_req,
        const std::string&                                    peer_id,
        std::shared_ptr<smpp_gateway>                         smpp_gateway
    );

    /**
     * @brief Processes a deliver_sm_resp received from an external client and sends it back to the pinex.
     *
     * @param smpp_gateway Shared pointer to the SMPP Gateway instance.
     * @param user_data_info Shared pointer to the `deliver_info` object containing delivery details about the request.
     * @param error SMPP command status error code received from the external client.
     */

    static bool process_resp(
        std::shared_ptr<smpp_gateway> smpp_gateway,
        std::shared_ptr<deliver_info> user_data_info,
        pa::smpp::command_status      error
    );
};
