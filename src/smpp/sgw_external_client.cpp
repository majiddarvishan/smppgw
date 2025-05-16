#include "sgw_external_client.h"

#include "src/smpp/deliver_sm.h"
#include "src/smpp/delivery_report.h"

#include "src/logging/sgw_logger.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

sgw_external_client::sgw_external_client(
    std::shared_ptr<smpp_gateway>            smpp_gateway,
    boost::asio::io_context*                 io_context,
    pa::config::manager*                     config_manager,
    const std::shared_ptr<pa::config::node>& config,
    const std::shared_ptr<pa::config::node>& prometheus_config,
    std::shared_ptr<prometheus::Registry> registry)
    : smpp_gateway_ { smpp_gateway }
    , config_manager_ { config_manager }
    , system_id_ { config->at("system_id")->get<std::string>() }
    , max_session_ { config->at("max_session")->get<int>() }
    , srr_state_generator_{config->at("status_report_state_generator")->get<bool>()}
    , srr_state_{config->at("status_report_state")->get<std::string>()}
    , system_type_ { config->at("system_type")->get<std::string>() }
    , password_ { config->at("password")->get<std::string>() }
    , require_password_checking_ { config->at("require_password_checking")->get<bool>() }
    , ignore_user_validity_period_ { config->at("ignore_user_validity_period")->get<bool>() }
    , max_submit_validity_period_ { config->at("submit_validity_period")->get<int>() }
    , max_delivery_report_validity_period_ { config->at("delivery_report_validity_period")->get<int>() }
    , timeout_sec_{ std::chrono::seconds(config->at("dialog_timeout")->get<uint64_t>()) }

    , config_obs_submit_resp_msg_id_base_{config_manager->on_replace(config->at("submit_resp_msg_id_base"), std::bind_front(&sgw_external_client::set_submit_resp_msg_id_base, this))}
    , config_obs_delivery_report_msg_id_base_{config_manager->on_replace(config->at("delivery_report_msg_id_base"), std::bind_front(&sgw_external_client::set_delivery_report_msg_id_base, this))}
    , config_obs_system_type_{config_manager->on_replace(config->at("system_type"), std::bind_front(&sgw_external_client::set_system_type, this))}
    , config_obs_require_password_checking_{config_manager->on_replace(config->at("require_password_checking"), std::bind_front(&sgw_external_client::set_require_password_checking, this))}
    //, config_obs_require_ip_checking_{config_manager->on_replace(config->at("require_ip_checking"), std::bind_front(&sgw_external_client::set_require_ip_checking, this))}
    //, config_obs_ip_mask_{config_manager->on_replace(config->at("ip_mask"), std::bind_front(&sgw_external_client::set_ip_mask, this))}
    , config_obs_ignore_user_validity_period_{config_manager->on_replace(config->at("ignore_user_validity_period"), std::bind_front(&sgw_external_client::set_ignore_user_validity_period, this))}
    , config_obs_submit_validity_period_{config_manager->on_replace(config->at("submit_validity_period"), std::bind_front(&sgw_external_client::set_submit_validity_period, this))}
    , config_obs_delivery_report_validity_period_{config_manager->on_replace(config->at("delivery_report_validity_period"), std::bind_front(&sgw_external_client::set_delivery_report_validity_period, this))}
    , config_obs_source_address_check_{config_manager->on_replace(config->at("source_address_check"), std::bind_front(&sgw_external_client::set_source_address_check, this))}
    , config_obs_source_ton_npi_check_{config_manager->on_replace(config->at("source_ton_npi_check"), std::bind_front(&sgw_external_client::set_source_ton_npi_check, this))}
    , config_obs_destination_address_check_{config_manager->on_replace(config->at("destination_address_check"), std::bind_front(&sgw_external_client::set_destination_address_check, this))}
    , config_obs_destination_ton_npi_check_{config_manager->on_replace(config->at("destination_ton_npi_check"), std::bind_front(&sgw_external_client::set_destination_ton_npi_check, this))}
    , config_obs_dcs_check_{config_manager->on_replace(config->at("dcs_check"), std::bind_front(&sgw_external_client::set_dcs_check, this))}
    , config_obs_black_white_check_{config_manager->on_replace(config->at("black_white_check"), std::bind_front(&sgw_external_client::set_black_white_check, this))}
    , config_obs_max_session_{config_manager->on_replace(config->at("max_session"), std::bind_front(&sgw_external_client::set_max_session, this))}
    , config_obs_srr_state_generator_{config_manager->on_replace(config->at("status_report_state_generator"), std::bind_front(&sgw_external_client::set_srr_state_generator, this))}
    , config_obs_srr_state_{config_manager->on_replace(config->at("status_report_state"), std::bind_front(&sgw_external_client::set_srr_state, this))}
    , bind_family_gauge_(prometheus::BuildGauge().Name("smpp_server_bind_status").Help("smpp server bind status parameters").Register(*registry))
    , bind_family_counter_(prometheus::BuildCounter().Name("smpp_server_bind_failed").Help("smpp server bind failed").Register(*registry))
    , submit_family_counter_(prometheus::BuildCounter().Name("smpp_server_submit").Help("smpp server submit parameters").Register(*registry))
    , submit_resp_family_counter_(prometheus::BuildCounter().Name("smpp_server_submit_resp").Help("smpp server submit_resp parameters").Register(*registry))
    , deliver_family_counter_(prometheus::BuildCounter().Name("smpp_server_deliver").Help("smpp server deliver parameters").Register(*registry))
    , deliver_resp_family_counter_(prometheus::BuildCounter().Name("smpp_server_deliver_resp").Help("smpp server deliver_resp parameters").Register(*registry))
    , delivery_report_family_counter_(prometheus::BuildCounter().Name("smpp_server_delivery_report").Help("smpp server delivery_report parameters").Register(*registry))
    , container_family_gauge_(prometheus::BuildGauge().Name("smpp_server_container_size").Help("smpp server container size").Register(*registry))

    , delivery_report_resp_family_counter_(prometheus::BuildCounter().Name("smpp_server_delivery_report_resp").Help("smpp server delivery_report_resp parameters").Register(*registry))
    , connected_connections_(add_gauge(bind_family_gauge_, prometheus_config->at("labels"), {
    { "name", "connected_connections" }, { "system_id", system_id_ }
}))
    , connection_reqs_failed_(add_counter(bind_family_counter_, prometheus_config->at("labels"), {
    { "name", "connection_reqs_failed" }, { "system_id", system_id_ }
}))
    , wait_for_resp_(add_gauge(container_family_gauge_, prometheus_config->at("labels"), {
    { "name", "wait_for_resp" }, { "system_id", system_id_ }, { "interface", "smpp" }
}))
    , submits_received_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "received" }, { "system_id", system_id_ }
}))
    , submits_required_dr_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "submits_require_srr" }, { "system_id", system_id_ }
}))
    , submits_encoding_default_alphabet_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "submits_default_encoding" }, { "category", "data_encoding" }, { "system_id", system_id_ }
}))
    , submits_encoding_8_bit_ascii_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "submits_8bit_ascii_encoding" }, { "category", "data_encoding" }, { "system_id", system_id_ }
}))
    , submits_encoding_8_bit_data_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "submits_8bit_data_encoding" }, { "category", "data_encoding" }, { "system_id", system_id_ }
}))
    , submits_encoding_16_bit_data_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "submits_16bit_data_encoding" }, { "category", "data_encoding" }, { "system_id", system_id_ }
}))
    , submits_invalid_data_encoding_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "submits_invalid_data_encoding" }, { "category", "data_encoding" }, { "system_id", system_id_ }
}))
    , submits_rejected_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
    { "name", "submits_rejected" }, { "system_id", system_id_ }
}))
    , submits_rejected_by_flow_control_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
        { "name", "submits_rejected_by_flow_control" }, { "system_id", system_id_ }
}))
    , submits_rejected_by_policy_(add_counter(submit_family_counter_, prometheus_config->at("labels"), {
        { "name", "submits_rejected_by_policy" }, { "system_id", system_id_ }
}))
    , submit_resp_status_ok_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "ok_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_timeout_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "timeout_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_invalid_data_encoding_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "invalid_data_encoding_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_invalid_source_address_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "invalid_src_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_invalid_source_address_ton_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "invalid_src_ton_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_invalid_source_address_npi_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "invalid_src_npi_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_invalid_destination_address_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "invalid_dst_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_invalid_destination_address_ton_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "invalid_dst_ton_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_invalid_destination_address_npi_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "invalid_dst_npi_status" }, { "system_id", system_id_ }
}))
    , submit_resp_status_others_error_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "other_error_status" }, { "system_id", system_id_ }
}))
    , send_submit_resp_successful_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_succeed" }, { "system_id", system_id_ }
}))
    , send_submit_resp_failed_(add_counter(submit_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_failed" }, { "system_id", system_id_ }
}))
    , send_deliver_successful_(add_counter(deliver_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_succeed" }, { "system_id", system_id_ }
}))
    , send_deliver_failed_(add_counter(deliver_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_failed" }, { "system_id", system_id_ }
}))
    , received_deliver_resp_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "received" }, { "system_id", system_id_ }
}))
    , rejected_deliver_resp_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "rejected" }, { "system_id", system_id_ }
}))
    , deliver_resp_status_timeout_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "timeout_status" }, { "system_id", system_id_ }
}))
    , deliver_resp_status_success_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "success_status" }, { "system_id", system_id_ }
}))
    , deliver_resp_status_fail_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "fail_status" }, { "system_id", system_id_ }
}))
    , send_deliver_resp_successful_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_succeed" }, { "system_id", system_id_ }
}))
    , send_deliver_resp_failed_(add_counter(deliver_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_failed" }, { "system_id", system_id_ }
}))
    , received_dr_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "received" }, { "system_id", system_id_ }
}))
    , dr_status_delivered_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "delivered_status" }, { "system_id", system_id_ }
}))
    , dr_status_expired_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "expired_status" }, { "system_id", system_id_ }
}))
    , dr_status_deleted_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "deleted_status" }, { "system_id", system_id_ }
}))
    , dr_status_undeliverable_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "undeliverable_status" }, { "system_id", system_id_ }
}))
    , dr_status_accepted_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "accepted_status" }, { "system_id", system_id_ }
}))
    , dr_status_unknown_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "unknown_status" }, { "system_id", system_id_ }
}))
    , dr_status_rejected_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "rejected_status" }, { "system_id", system_id_ }
}))
    , send_dr_successful_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_succeed" }, { "system_id", system_id_ }
}))
    , send_dr_failed_(add_counter(delivery_report_family_counter_, prometheus_config->at("labels"), {
    { "name", "sending_failed" }, { "system_id", system_id_ }
}))
    , received_dr_resp_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "received_resp" }, { "system_id", system_id_ }
}))
    , rejected_dr_resp_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "rejected_resp" }, { "system_id", system_id_ }
}))
    , dr_resp_status_timeout_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "resp_timeout_status" }, { "system_id", system_id_ }
}))
    , dr_resp_status_successful_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "resp_success_status" }, { "system_id", system_id_ }
}))
    , dr_resp_status_failed_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "resp_fail_status" }, { "system_id", system_id_ }
}))
    , send_dr_resp_successful_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "resp_sending_succeed" }, { "system_id", system_id_ }
}))
    , send_dr_resp_failed_(add_counter(delivery_report_resp_family_counter_, prometheus_config->at("labels"), {
    { "name", "resp_sending_failed" }, { "system_id", system_id_ }
}))
{
    uptime_.store(0);
    srand(time(0));

    for (const auto& value : config->at("permitted_bind_types")->nodes()) {

        if("TX" == value->get<std::string>())
            permitted_bind_types_.emplace_back(pa::smpp::bind_type::transmitter);

        else if("RX" == value->get<std::string>())
            permitted_bind_types_.emplace_back(pa::smpp::bind_type::receiver);

        else if("TRX" == value->get<std::string>())
            permitted_bind_types_.emplace_back(pa::smpp::bind_type::transceiver);

        else
            LOG_DEBUG("sgw_external_client::sgw_external_client: '{}' isn't valid bind_type", value->get<std::string>());
    }

    try
    {
        ip_mask_ = config->at("ip_mask")->get<std::string>();
    }
    catch(...)
    {
        ip_mask_ = "";
    }

    try
    {
        require_ip_checking_ = config->at("require_ip_checking")->get<bool>();
    }
    catch(...)
    {
        require_ip_checking_ = false;
    }

    try
    {
        for(const auto& value: config->at("ip_addresses")->nodes())
           ip_addresses_.emplace_back(value->get<std::string>());
    }
    catch(...)
    {
    }

    set_submit_resp_msg_id_base(config->at("submit_resp_msg_id_base"));
    set_delivery_report_msg_id_base(config->at("delivery_report_msg_id_base"));

    packet_expirator_ = std::make_shared<io::expirator<uint64_t, std::shared_ptr<deliver_info>>>(
        io_context,
        std::chrono::milliseconds{ 1 },
        std::bind_front(&sgw_external_client::on_packet_expire, this));

    packet_expirator_->start();

    receive_flow_control_ = std::make_shared<io::flow_control>(io_context, config_manager, config->at("receive_flow_control"), [this]() {
        for(auto session:binded_sessions_)
            session->resume_receiving();
    });

    send_flow_control_ = std::make_shared<io::flow_control>(io_context, config_manager, config->at("send_flow_control"), [this]() { send_process(); });
    send_flow_control_->wait(std::chrono::steady_clock::now() + std::chrono::microseconds{ 1 });

    policy_commands_.clear();

    if(config->at("source_address_check")->get<bool>())
    {
        policy_commands_.insert(pa::paper::proto::Request::SOURCE_ADDRESS_CHECK);
    }

    if(config->at("source_ton_npi_check")->get<bool>())
    {
        policy_commands_.insert(pa::paper::proto::Request::SOURCE_TON_NPI_CHECK);
    }

    if(config->at("destination_address_check")->get<bool>())
    {
        policy_commands_.insert(pa::paper::proto::Request::DESTINATION_ADDRESS_CHECK);
    }

    if(config->at("destination_ton_npi_check")->get<bool>())
    {
        policy_commands_.insert(pa::paper::proto::Request::DESTINATION_TON_NPI_CHECK);
    }

    if(config->at("dcs_check")->get<bool>())
    {
        policy_commands_.insert(pa::paper::proto::Request::DCS_CHECK);
    }

    if(config->at("black_white_check")->get<bool>())
    {
        policy_commands_.insert(pa::paper::proto::Request::BLACK_WHITE_CHECK);
    }
}

sgw_external_client::~sgw_external_client()
{
}

void sgw_external_client::stop()
{
    for(auto& session : binded_sessions_)
        session->unbind(true /*force*/);

    remove_gauge(bind_family_gauge_, &connected_connections_);

    remove_counter(bind_family_counter_, &connection_reqs_failed_);

    remove_counter(submit_family_counter_, &submits_received_);
    remove_counter(submit_family_counter_, &submits_required_dr_);
    remove_counter(submit_family_counter_, &submits_encoding_default_alphabet_);
    remove_counter(submit_family_counter_, &submits_encoding_8_bit_ascii_);
    remove_counter(submit_family_counter_, &submits_encoding_8_bit_data_);
    remove_counter(submit_family_counter_, &submits_encoding_16_bit_data_);
    remove_counter(submit_family_counter_, &submits_invalid_data_encoding_);
    remove_counter(submit_family_counter_, &submits_rejected_);
    remove_counter(submit_family_counter_, &submits_rejected_by_flow_control_);
    remove_counter(submit_family_counter_, &submits_rejected_by_policy_);

    remove_counter(submit_resp_family_counter_, &submit_resp_status_ok_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_timeout_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_invalid_data_encoding_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_invalid_source_address_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_invalid_source_address_ton_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_invalid_source_address_npi_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_invalid_destination_address_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_invalid_destination_address_ton_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_invalid_destination_address_npi_);
    remove_counter(submit_resp_family_counter_, &submit_resp_status_others_error_);
    remove_counter(submit_resp_family_counter_, &send_submit_resp_successful_);
    remove_counter(submit_resp_family_counter_, &send_submit_resp_failed_);

    remove_counter(deliver_family_counter_, &send_deliver_successful_);
    remove_counter(deliver_family_counter_, &send_deliver_failed_);

    remove_counter(deliver_resp_family_counter_, &received_deliver_resp_);
    remove_counter(deliver_resp_family_counter_, &rejected_deliver_resp_);
    remove_counter(deliver_resp_family_counter_, &deliver_resp_status_timeout_);
    remove_counter(deliver_resp_family_counter_, &deliver_resp_status_success_);
    remove_counter(deliver_resp_family_counter_, &deliver_resp_status_fail_);
    remove_counter(deliver_resp_family_counter_, &send_deliver_resp_successful_);
    remove_counter(deliver_resp_family_counter_, &send_deliver_resp_failed_);

    remove_counter(delivery_report_family_counter_, &received_dr_);
    remove_counter(delivery_report_family_counter_, &dr_status_delivered_);
    remove_counter(delivery_report_family_counter_, &dr_status_expired_);
    remove_counter(delivery_report_family_counter_, &dr_status_deleted_);
    remove_counter(delivery_report_family_counter_, &dr_status_undeliverable_);
    remove_counter(delivery_report_family_counter_, &dr_status_accepted_);
    remove_counter(delivery_report_family_counter_, &dr_status_unknown_);
    remove_counter(delivery_report_family_counter_, &dr_status_rejected_);
    remove_counter(delivery_report_family_counter_, &send_dr_successful_);
    remove_counter(delivery_report_family_counter_, &send_dr_failed_);

    remove_gauge(container_family_gauge_, &wait_for_resp_);

    remove_counter(delivery_report_resp_family_counter_, &received_dr_resp_);
    remove_counter(delivery_report_resp_family_counter_, &rejected_dr_resp_);
    remove_counter(delivery_report_resp_family_counter_, &dr_resp_status_timeout_);
    remove_counter(delivery_report_resp_family_counter_, &dr_resp_status_successful_);
    remove_counter(delivery_report_resp_family_counter_, &dr_resp_status_failed_);
    remove_counter(delivery_report_resp_family_counter_, &send_dr_resp_successful_);
    remove_counter(delivery_report_resp_family_counter_, &send_dr_resp_failed_);
}

void sgw_external_client::set_session(std::shared_ptr<pa::smpp::session> session)
{
    binded_sessions_.insert(session);
}

void sgw_external_client::on_packet_expire(uint64_t, std::shared_ptr<deliver_info> user_data)
{
    LOG_INFO("packet is timeout on connection {}", system_id_);

    // if (user_data->is_report_)
    //     delivery_report_timeout_counter_.Increment();
    // else
    //     deliver_timeout_counter_.Increment();

    user_data->error_ = pa::smpp::command_status::rtimeout;
    process_deliver_resp(user_data);
}

// mshadowQ: if multiple bind_type supported why "bind_type" input parameter is single value?
pa::smpp::command_status sgw_external_client::check_permision(const std::string&  system_type,
                                                              const std::string&  password,
                                                              const std::string&  ip,
                                                              pa::smpp::bind_type bind_type)
{
    if(max_session_ == binded_sessions_.size())
    {
        LOG_ERROR("max session limit exceed '{}'.", system_id_);
        connection_reqs_failed_.Increment();
        return pa::smpp::command_status::rbindfail;
    }

    if(system_type_ != system_type)
    {
        LOG_ERROR("Invalid system_type '{}'.", system_type);
        connection_reqs_failed_.Increment();
        return pa::smpp::command_status::rinvsystyp;
    }

    if(require_password_checking_ && (password_ != password))
    {
        LOG_ERROR("Invalid Password '{}'.", password);
        connection_reqs_failed_.Increment();
        return pa::smpp::command_status::rinvpaswd;
    }

    auto itr1 = std::find(permitted_bind_types_.begin(), permitted_bind_types_.end(), bind_type);
    if(itr1 == permitted_bind_types_.end())
    {
        LOG_ERROR("Illegal Bind Type '{}'.", static_cast<int>(bind_type));
        connection_reqs_failed_.Increment();
        return pa::smpp::command_status::rinvbndsts;
    }

    auto itr2 = std::find(ip_addresses_.begin(), ip_addresses_.end(), ip);
    if(require_ip_checking_ && (itr2 == ip_addresses_.end()))
    {
        LOG_ERROR("Invalid IP address '{}'.", ip);
        connection_reqs_failed_.Increment();
        return pa::smpp::command_status::rinvip;
    }

    if(itr2 != ip_addresses_.end())
        LOG_DEBUG("{} ip address is bind successfully.", (*itr2));
    else
        LOG_DEBUG("bind by ignoring IP");

    uptime_.store(1);
    connected_connections_.Increment();

    return pa::smpp::command_status::rok;
}

void sgw_external_client::on_session_close(std::shared_ptr<pa::smpp::session> session, std::optional<std::string> error)
{
    auto it = binded_sessions_.find(session);
    if(it == binded_sessions_.end())
    {
        LOG_ERROR("session of client {} is not valid in close scenario", system_id_);
        return;
    }

    binded_sessions_.erase(it);

    if(error)
        LOG_WARN("session {} has been closed on error {}", system_id_, error.value().c_str());
    else
        LOG_INFO("session {} has been closed gracefully", system_id_);

    connected_connections_.Decrement();

    // binded_session_.reset();
    if(binded_sessions_.empty())
    {
        uptime_.store(0);
    }

    packet_expirator_->expire_all();
}

void sgw_external_client::on_session_request(std::shared_ptr<pa::smpp::session> session, pa::smpp::request&& request, uint32_t sequence_number)
{
    std::visit(
        [&](auto&& req) {
        using request_type = std::decay_t<decltype(req)>;

        if constexpr(std::is_same_v<request_type, pa::smpp::submit_sm>)
        {
            process_submit_req(smpp_gateway_, shared_from_this(), std::move(req), sequence_number, session);
        }
        else if constexpr(std::is_same_v<request_type, pa::smpp::query_sm>)
        {
            LOG_ERROR("receive query_sm");
        }
        else if constexpr(std::is_same_v<request_type, pa::smpp::replace_sm>)
        {
            LOG_ERROR("receive replace_sm");
        }
        else if constexpr(std::is_same_v<request_type, pa::smpp::cancel_sm>)
        {
            LOG_ERROR("receive cancel_sm");
        }
        else
        {
            LOG_ERROR("invalid packet type");
        }
    },
        request);
}

void sgw_external_client::on_session_response(std::shared_ptr<pa::smpp::session> session,
                                              pa::smpp::response&&     response_packet,
                                              uint32_t                 sequence_number,
                                              pa::smpp::command_status command_status)
{
    std::visit(
        [&](auto&& resp) {
        using resonse_type = std::decay_t<decltype(resp)>;

        if constexpr(std::is_same_v<resonse_type, pa::smpp::deliver_sm_resp>)
        {
            auto u = packet_expirator_->get_info(sequence_number);
            if (u == std::nullopt)
            {
                LOG_ERROR("could not find user_data for sequence {} to send back response", sequence_number);
                //todo:
                // if(true == orig_deliver_info->is_report_)
                //     rejected_deliver_resp_.Increment();
                // else
                //     rejected_dr_resp_.Increment();

                return;
            }

            auto orig_deliver_info = u.value();
            packet_expirator_->remove(sequence_number);
            wait_for_resp_.Decrement();

            orig_deliver_info->error_ = command_status;

            process_deliver_resp(orig_deliver_info);
        }
        else
        {
            LOG_ERROR("invalid packet type");
        }
    },
        response_packet);
}

void sgw_external_client::process_deliver_resp(std::shared_ptr<deliver_info> orig_deliver_info)
{
    if(orig_deliver_info->is_report_)
    {
        received_dr_resp_.Increment();

        if(orig_deliver_info->error_ == pa::smpp::command_status::rtimeout)
        {
            dr_resp_status_timeout_.Increment();
        }
        else
        {
            if(orig_deliver_info->error_ == pa::smpp::command_status::rok)
            {
                dr_resp_status_successful_.Increment();
            }
            else
            {
                dr_resp_status_failed_.Increment();
            }
        }

        bool result = delivery_report::process_resp(smpp_gateway_, orig_deliver_info, orig_deliver_info->error_);
        if(true == result)
        {
            send_dr_resp_successful_.Increment();
        }
        else
        {
            send_dr_resp_failed_.Increment();
        }
    }
    else
    {
        received_deliver_resp_.Increment();

        if(orig_deliver_info->error_ == pa::smpp::command_status::rtimeout)
        {
            deliver_resp_status_timeout_.Increment();
        }
        else
        {
            received_deliver_resp_.Increment();

            if(orig_deliver_info->error_ == pa::smpp::command_status::rok)
            {
                deliver_resp_status_success_.Increment();
            }
            else
            {
                deliver_resp_status_fail_.Increment();
            }
        }

        bool result = deliver_sm::process_resp(smpp_gateway_, orig_deliver_info, orig_deliver_info->error_);
        if(true == result)
        {
            send_deliver_resp_successful_.Increment();
        }
        else
        {
            send_deliver_resp_failed_.Increment();
        }
    }
}

void sgw_external_client::on_session_deserialization_error(std::shared_ptr<pa::smpp::session> session, const std::string& error, pa::smpp::command_id command_id, std::span<const uint8_t> body)
{

    LOG_ERROR("error in pdu deserialization, error: {} command_id: {} body-size: {}",
              static_cast<uint32_t>(command_id), error, body.size());

    LOG_HEX(spdlog::level::err, body);
}

void sgw_external_client::on_session_send_buf_available(std::shared_ptr<pa::smpp::session> session)
{
    LOG_INFO("send buf of client: {} become available again", system_id_.c_str());
    //(*it)->resume_receiving();
}

// mshadow: todo: refactor and optimize this function implementation because it does several task together and Solid principles have not been observed
void sgw_external_client::process_submit_req(
    std::shared_ptr<smpp_gateway>        smpp_gateway,
    std::shared_ptr<sgw_external_client> ext_client,
    pa::smpp::submit_sm&&                request,
    uint32_t                             sequence_number,
    std::shared_ptr<pa::smpp::session>   session)
{
    LOG_DEBUG("process received submit(AO_REQ).");

    auto user_data_info = std::make_shared<submit_info>();
    user_data_info->international_source_address_ = pa::smpp::convert_to_international(request.source_addr_ton, request.source_addr);
    user_data_info->international_dest_address_ = pa::smpp::convert_to_international(request.dest_addr_ton, request.dest_addr);
    user_data_info->originating_sequence_number_ = sequence_number;
    user_data_info->originating_ext_client_ = ext_client;
    user_data_info->originating_session_ = session;

    sgw_logger::getInstance()->trace_message(
        SMSC::Protobuf::AO_REQ_TYPE,
        user_data_info->smsc_unique_id_,
        "",    //msg_id
        SMSC::Trace::Protobuf::ReceiveSubmit,
        ext_client->get_system_id(),
        "",    //destination_clinet_id
        user_data_info->international_source_address_,
        user_data_info->international_dest_address_,
        0, //error
        "success");

    ext_client->submits_received_.Increment();

    const auto wait_time = receive_flow_control_->check();
    if (wait_time)
    {
        if (receive_flow_control_->drop_packet())
        {
            submits_rejected_by_flow_control_.Increment();
            submits_rejected_.Increment();

            user_data_info->error_ = pa::smpp::command_status::rthrottled;

            submit_sm::send_resp(user_data_info);
            return;
        }

        for(auto& session : binded_sessions_)
            session->pause_receiving();

        receive_flow_control_->wait(std::chrono::steady_clock::now() + std::chrono::microseconds{ wait_time });
    }

    try
    {
        //mshadow:todo: monitoring variable should be handle in external_client class and all monitoring variable must be private
        auto [header, body] = pa::smpp::unpack_short_message(request.esm_class, request.data_coding, request.short_message);
        user_data_info->data_coding_type_ = pa::smpp::extract_unicode(request.data_coding);
        switch(user_data_info->data_coding_type_)
        {
            case pa::smpp::data_coding_unicode::ascii_7_bit:
            {
                user_data_info->body = pa::smpp::convert_gsm_to_ucs2(body);
                ext_client->submits_encoding_default_alphabet_.Increment();
                break;
            }

            case pa::smpp::data_coding_unicode::ascii_8_bit:
            {
                user_data_info->body = pa::smpp::convert_ascii_to_ucs2(body);
                ext_client->submits_encoding_8_bit_ascii_.Increment();
                break;
            }

            case pa::smpp::data_coding_unicode::binary:
            {
                user_data_info->body = body;
                ext_client->submits_encoding_8_bit_data_.Increment();
                break;
            }

            case pa::smpp::data_coding_unicode::ucs2:
            {
                user_data_info->body = body;
                ext_client->submits_encoding_16_bit_data_.Increment();
                break;
            }

            default:
                LOG_ERROR("invalid data_encoding('{}')", static_cast<int>(user_data_info->data_coding_type_));
                ext_client->submits_invalid_data_encoding_.Increment();
                LOG_DEBUG("The submission was rejected and could not be sent.");
                ext_client->submits_rejected_.Increment();
                return;
        }

        boost::uuids::uuid new_uuid = boost::uuids::random_generator()();
        std::string uuid_str = boost::uuids::to_string(new_uuid);

        user_data_info->smsc_unique_id_ = uuid_str;

        const auto [ip_address, port] = session->remote_endpoint();

        if(request.registered_delivery.smsc_delivery_receipt != pa::smpp::smsc_delivery_receipt::no)
        {
            ext_client->submits_required_dr_.Increment();
        }

        if (srr_state_generator_)
        {
            if (srr_state_ == "never")
                request.registered_delivery.smsc_delivery_receipt = pa::smpp::smsc_delivery_receipt::no;
            else if (srr_state_ == "always")
                request.registered_delivery.smsc_delivery_receipt = pa::smpp::smsc_delivery_receipt::both;
            else if (srr_state_ == "on_failed")
                request.registered_delivery.smsc_delivery_receipt = pa::smpp::smsc_delivery_receipt::failed;
            else if (srr_state_ == "on_succeed")
                request.registered_delivery.smsc_delivery_receipt = pa::smpp::smsc_delivery_receipt::succeed;
        }

        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        user_data_info->submit_req_received_time_ = microseconds;
        user_data_info->source_connection_ = ext_client->get_system_id();     //TODO
        user_data_info->number_of_parts_ = header.get_multi_part_data().number_of_parts_;
        user_data_info->concat_ref_num_ = header.get_multi_part_data().concat_sm_ref_num_;
        user_data_info->part_number_ = header.get_multi_part_data().sequence_number_;
        user_data_info->is_multi_part_ = header.get_multi_part_data().number_of_parts_ > 1 ? true : false;
        user_data_info->system_type_ = ext_client->get_system_type();
        user_data_info->source_ip_ = ip_address;
        user_data_info->request = request;
        user_data_info->header = header.serialize();

        if(policy_commands_.size())
        {
            sgw_logger::getInstance()->trace_message(
                SMSC::Protobuf::AO_REQ_TYPE,
                user_data_info->smsc_unique_id_,
                "",    //msg_id
                SMSC::Trace::Protobuf::PolicyRequest,
                ext_client->get_system_id(),
                "",    //destination_clinet_id
                user_data_info->international_source_address_,
                user_data_info->international_dest_address_,
                0, //error
                "success");

            if(false == smpp_gateway->check_policies(ext_client->get_system_id(), user_data_info, policy_commands_))
            {
                user_data_info->error_ = pa::smpp::command_status::rsyserr;
                ext_client->submits_rejected_.Increment();
                submit_sm::send_resp(user_data_info);
            }

            return;
        }

        user_data_info->error_ = pa::smpp::command_status::rok;
        submit_sm::on_check_policies_responce(smpp_gateway, user_data_info, false);
        return;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR("catch an exception when processing submit, {}", ex.what());
    }
    catch (...)
    {
        std::exception_ptr p = std::current_exception();
        LOG_ERROR("catch an exception when processing submit, {}", (p ? p.__cxa_exception_type()->name() : "null"));
    }

    user_data_info->error_ = pa::smpp::command_status::rsyserr;

    sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::AO_REQ_TYPE,
            user_data_info->smsc_unique_id_,
            "",            /*msg_id*/
            SMSC::Trace::Protobuf::SendFailed,
            user_data_info->originating_ext_client_->get_system_id(),
            "",            /*destination_client_id*/
            user_data_info->international_source_address_,
            user_data_info->international_dest_address_,
            (int)user_data_info->error_,
            "");

    //todo:Majid Darvishan: check parameters of user_data is already filled
    submit_sm::send_resp(user_data_info);
}

bool sgw_external_client::send_submit_resp(std::shared_ptr<submit_info> user_data)
{
    LOG_DEBUG("send submit_sm_resp(AO_RESP_TYPE)");

    if(binded_sessions_.empty())
    {
        LOG_ERROR("send packet on closed connection");
        send_submit_resp_failed_.Increment();
        return false;
    }

    switch(user_data->error_)
    {
        case pa::smpp::command_status::rok:
        {
            submit_resp_status_ok_.Increment();
            break;
        }

        case pa::smpp::command_status::rtimeout:
        {
            submit_resp_status_timeout_.Increment();
            break;
        }

        case pa::smpp::command_status::rinvdcs:
        {
            submit_resp_status_invalid_data_encoding_.Increment();
            submits_rejected_by_policy_.Increment();
            submits_rejected_.Increment();
            break;
        }

        case pa::smpp::command_status::rinvsrcadr:
        {
            submit_resp_status_invalid_source_address_.Increment();
            submits_rejected_by_policy_.Increment();
            submits_rejected_.Increment();
            break;
        }

        case pa::smpp::command_status::rinvsrcton:
        {
            submit_resp_status_invalid_source_address_ton_.Increment();
            submits_rejected_by_policy_.Increment();
            submits_rejected_.Increment();
            break;
        }

        case pa::smpp::command_status::rinvsrcnpi:
        {
            submit_resp_status_invalid_source_address_npi_.Increment();
            submits_rejected_by_policy_.Increment();
            submits_rejected_.Increment();
            break;
        }

        case pa::smpp::command_status::rinvdstadr:
        {
            submit_resp_status_invalid_destination_address_.Increment();
            submits_rejected_by_policy_.Increment();
            submits_rejected_.Increment();

            break;
        }

        case pa::smpp::command_status::rinvdstton:
        {
            submit_resp_status_invalid_destination_address_ton_.Increment();
            submits_rejected_by_policy_.Increment();
            submits_rejected_.Increment();
            break;
        }

        case pa::smpp::command_status::rinvdstnpi:
        {
            submit_resp_status_invalid_destination_address_npi_.Increment();
            submits_rejected_by_policy_.Increment();
            submits_rejected_.Increment();
            break;
        }

        default:
        {
            submit_resp_status_others_error_.Increment();
        }
    }

    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    user_data->submit_resp_sent_time_ = microseconds;
    try
    {
        user_data->originating_session_->send(pa::smpp::submit_sm_resp{.message_id = user_data->message_id_ }, user_data->originating_sequence_number_, user_data->error_);

        send_submit_resp_successful_.Increment();
        return true;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR("catch an exception on send, {}", ex.what());
    }
    catch (...)
    {
        std::exception_ptr p = std::current_exception();
        LOG_ERROR("catch an exception on send, {}", (p ? p.__cxa_exception_type()->name() : "null"));
    }

    LOG_ERROR("Could not send submit_resp to client {}", system_id_);

    send_submit_resp_failed_.Increment();

    return false;
}

void sgw_external_client::flow_controlled_send_deliver(std::shared_ptr<deliver_info> deliver_info)
{
    send_queue_.push_back(deliver_info);
}

void sgw_external_client::send_process()
{
    while (!send_queue_.empty())
    {
        auto wait_time = send_flow_control_->check();
        if (wait_time)
        {
            send_flow_control_->wait(std::chrono::steady_clock::now() + std::chrono::microseconds{ wait_time });
            return;
        }

        auto user_data = send_queue_.front();
        send_queue_.pop_front();

        send_deliver_sm(user_data);
    }

    send_flow_control_->wait(std::chrono::steady_clock::now() + std::chrono::milliseconds{ 1 });
}

void sgw_external_client::send_deliver_sm(std::shared_ptr<deliver_info> deliver_info)
{
    LOG_DEBUG("send deliver packet on system-id = '{}'", system_id_);

    if(binded_sessions_.empty())
    {
        LOG_ERROR("send packet on closed connection {}", system_id_);

        if(deliver_info->is_report_)
            send_dr_failed_.Increment();
        else
            send_deliver_failed_.Increment();

        deliver_info->error_ = pa::smpp::command_status::dst_esme_not_bound;
        deliver_sm::process_resp(smpp_gateway_, deliver_info, pa::smpp::command_status::dst_esme_not_bound);
        return;
    }

    auto r = rand() % binded_sessions_.size();
    auto it = std::begin(binded_sessions_);
    std::advance(it, r);
    auto selected_session = *it;

    try
    {
        const auto [ip_address, port] = selected_session->remote_endpoint();
        deliver_info->destination_ip_ = ip_address;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR("catch an exception on send_deliver_sm, {}", ex.what());
        deliver_info->error_ = pa::smpp::command_status::rsyserr;
        deliver_sm::process_resp(smpp_gateway_, deliver_info, pa::smpp::command_status::rsyserr);
        return;
    }
    catch (...)
    {
        std::exception_ptr p = std::current_exception();
        LOG_ERROR("catch an exception on send_deliver_sm, {}", (p ? p.__cxa_exception_type()->name() : "null"));
        deliver_info->error_ = pa::smpp::command_status::rsyserr;
        deliver_sm::process_resp(smpp_gateway_, deliver_info, pa::smpp::command_status::rsyserr);
        return;
    }

    if(deliver_info->is_report_)
    {
        LOG_DEBUG("send dr to client {}", system_id_);

        auto sa = pa::smpp::convert_to_international((pa::smpp::ton)deliver_info->dr_request->mutable_smpp()->source_addr_ton(), deliver_info->dr_request->mutable_smpp()->source_addr());
        auto da = pa::smpp::convert_to_international((pa::smpp::ton)deliver_info->dr_request->mutable_smpp()->dest_addr_ton(), deliver_info->dr_request->mutable_smpp()->dest_addr());

        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::DR_REQ_TYPE,
            deliver_info->dr_request->smsc_unique_id(),
            "",    /*msg_id*/
            SMSC::Trace::Protobuf::SendDeliveryReport,
            deliver_info->source_connection_,
            get_system_id(),
            sa,
            da,
            0 /*error*/,
            "success");

        //todo: is this corret?
        received_dr_.Increment();

        std::string dr_status { deliver_info->dr_status_ };
        std::transform(dr_status.begin(), dr_status.end(), dr_status.begin(), ::toupper);

        if(dr_status.compare("DELIVRD") == 0)
        {
            dr_status_delivered_.Increment();
        }
        else if(dr_status.compare("EXPIRED") == 0)
        {
            dr_status_expired_.Increment();
        }
        else if(dr_status.compare("DELETED") == 0)
        {
            dr_status_deleted_.Increment();
        }
        else if(dr_status.compare("UNDELIV") == 0)
        {
            dr_status_undeliverable_.Increment();
        }
        else if(dr_status.compare("ACCEPTD") == 0)
        {
            dr_status_accepted_.Increment();
        }
        else if(dr_status.compare("UNKNOWN") == 0)
        {
            dr_status_unknown_.Increment();
        }
        else if(dr_status.compare("REJECTD") == 0)
        {
            dr_status_rejected_.Increment();
        }
        else
        {
            LOG_ERROR("The delivery report status '{}' is not valid?!", dr_status);
        }

        try
        {
            auto seq_no = selected_session->send(pa::smpp::deliver_sm{
            .service_type = deliver_info->dr_request->smpp().service_type(),
            .source_addr_ton = (pa::smpp::ton)deliver_info->dr_request->smpp().source_addr_ton(),
            .source_addr_npi = (pa::smpp::npi)deliver_info->dr_request->smpp().source_addr_npi(),
            .source_addr = deliver_info->dr_request->smpp().source_addr(),
            .dest_addr_ton = (pa::smpp::ton)deliver_info->dr_request->smpp().dest_addr_ton(),
            .dest_addr_npi = (pa::smpp::npi)deliver_info->dr_request->smpp().dest_addr_npi(),
            .dest_addr = deliver_info->dr_request->smpp().dest_addr(),
            .esm_class = pa::smpp::esm_class::from_u8(deliver_info->dr_request->smpp().esm_class()),
            .protocol_id = (uint8_t)deliver_info->dr_request->smpp().protocol_id(),
            .priority_flag = (pa::smpp::priority_flag)deliver_info->dr_request->smpp().priority_flag(),
            .schedule_delivery_time = "",
            .validity_period = "",
            .registered_delivery = pa::smpp::registered_delivery::from_u8(deliver_info->dr_request->smpp().registered_delivery()),
            .replace_if_present_flag = (pa::smpp::replace_if_present_flag)deliver_info->dr_request->smpp().replace_if_present_flag(),
            .data_coding = (pa::smpp::data_coding)deliver_info->dr_request->body().data_coding(),
            .sm_default_msg_id = (uint8_t)deliver_info->dr_request->smpp().sm_default_msg_id(),
            .short_message = deliver_info->dr_request->body().short_message(),
            });

            if(seq_no)
            {
                send_dr_successful_.Increment();
                packet_expirator_->add(seq_no, timeout_sec_, deliver_info);
                wait_for_resp_.Increment();
                return;
            }
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR("catch an exception on send, {}", ex.what());
        }
        catch (...)
        {
            std::exception_ptr p = std::current_exception();
            LOG_ERROR("catch an exception on send, {}", (p ? p.__cxa_exception_type()->name() : "null"));
        }

        LOG_ERROR("Could not send dr to client {}", system_id_);

        send_dr_failed_.Increment();
        deliver_info->error_ = pa::smpp::command_status::rsyserr;

        sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::DR_REQ_TYPE,
            deliver_info->dr_request->smsc_unique_id(),
            "",    /*msg_id*/
            SMSC::Trace::Protobuf::SendFailed,
            deliver_info->source_connection_,
            get_system_id(),
            sa,
            da,
            (int)pa::smpp::command_status::rsyserr,
            "");

        deliver_sm::process_resp(smpp_gateway_, deliver_info, pa::smpp::command_status::rsyserr);

        return;
    }

    LOG_DEBUG("send deliver_sm to client {}", system_id_);

    auto sa = pa::smpp::convert_to_international((pa::smpp::ton)deliver_info->request->mutable_smpp()->source_addr_ton(), deliver_info->request->mutable_smpp()->source_addr());
    auto da = pa::smpp::convert_to_international((pa::smpp::ton)deliver_info->request->mutable_smpp()->dest_addr_ton(), deliver_info->request->mutable_smpp()->dest_addr());

    sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::AT_REQ_TYPE,
            deliver_info->request->smsc_unique_id(),
            "",    /*msg_id*/
            SMSC::Trace::Protobuf::SendDeliver,
            deliver_info->source_connection_,
            get_system_id(),
            sa,
            da,
            0 /*error*/,
            "success");

    // auto udh = pa::smpp::user_data_header(deliver_info->request->body().header());
    auto udh = pa::smpp::user_data_header();
    if(deliver_info->request->body().sar_total_segments() > 1)
    {
        pa::smpp::user_data_header::multi_part_data mpd {
            .concat_sm_ref_num_ = (uint16_t)deliver_info->request->body().sar_msg_ref_num(),
            .number_of_parts_ = (uint8_t)deliver_info->request->body().sar_total_segments(),
            .sequence_number_ = (uint8_t)deliver_info->request->body().sar_segment_seqnum(),
        };

        udh.set_multi_part_data(mpd);
    }
    auto body = deliver_info->request->body().short_message();

    auto dct = pa::smpp::extract_unicode((pa::smpp::data_coding)deliver_info->request->body().data_coding());
    //todo:majid darvishan: better implementatin
    if((dct == pa::smpp::data_coding_unicode::ascii_8_bit) || (dct == pa::smpp::data_coding_unicode::ascii_7_bit))
    {
        std::string latin1;
        for(unsigned i = 0; i < body.length(); i += 2)
        {
            if(body[i] == 0)
            {
                latin1.push_back(body[i + 1]);
            }
            else
            {
                latin1.push_back(' ');
            }
        }
        body = latin1;
    }

    try
    {
        auto msg_body = pa::smpp::pack_short_message(udh, body, (pa::smpp::data_coding)deliver_info->request->body().data_coding());
        bool udhi = (deliver_info->request->body().sar_total_segments() > 1);

        auto seq_no = selected_session->send(
            pa::smpp::deliver_sm{ .service_type = deliver_info->request->smpp().service_type(),
            .source_addr_ton = (pa::smpp::ton)deliver_info->request->smpp().source_addr_ton(),
            .source_addr_npi = (pa::smpp::npi)deliver_info->request->smpp().source_addr_npi(),
            .source_addr = deliver_info->request->smpp().source_addr(),
            .dest_addr_ton = (pa::smpp::ton)deliver_info->request->smpp().dest_addr_ton(),
            .dest_addr_npi = (pa::smpp::npi)deliver_info->request->smpp().dest_addr_npi(),
            .dest_addr = deliver_info->request->smpp().dest_addr(),
            .esm_class = pa::smpp::esm_class::from_u8(deliver_info->request->smpp().esm_class()),
            .protocol_id = (uint8_t)deliver_info->request->smpp().protocol_id(),
            .priority_flag = (pa::smpp::priority_flag)deliver_info->request->smpp().priority_flag(),
            .schedule_delivery_time = "",
            .validity_period = "",
            .registered_delivery = pa::smpp::registered_delivery::from_u8(deliver_info->request->smpp().registered_delivery()),
            .replace_if_present_flag = (pa::smpp::replace_if_present_flag)deliver_info->request->smpp().replace_if_present_flag(),
            .data_coding = (pa::smpp::data_coding)deliver_info->request->body().data_coding(),
            .sm_default_msg_id = (uint8_t)deliver_info->request->smpp().sm_default_msg_id(),
            .short_message = msg_body,
        });

        if(seq_no)
        {
            send_deliver_successful_.Increment();
            packet_expirator_->add(seq_no, timeout_sec_, deliver_info);
            wait_for_resp_.Increment();
            return;
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR("catch an exception on send, {}", ex.what());
    }
    catch (...)
    {
        std::exception_ptr p = std::current_exception();
        LOG_ERROR("catch an exception on send, {}", (p ? p.__cxa_exception_type()->name() : "null"));
    }

    LOG_ERROR("Could not send deliver_sm to client {}", system_id_);

    send_deliver_failed_.Increment();
    deliver_info->error_ = pa::smpp::command_status::rsyserr;

    sgw_logger::getInstance()->trace_message(
            SMSC::Protobuf::AT_REQ_TYPE,
            deliver_info->request->smsc_unique_id(),
            "",    /*msg_id*/
            SMSC::Trace::Protobuf::SendFailed,
            deliver_info->source_connection_,
            get_system_id(),
            sa,
            da,
            (int)pa::smpp::command_status::rsyserr,
            "");

        deliver_sm::process_resp(smpp_gateway_, deliver_info, pa::smpp::command_status::rsyserr);

    return;
}

SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE sgw_external_client::get_submit_resp_msg_id_base() const
{
    return submit_resp_msg_id_base_;
}

SMSC::Protobuf::SMPP_MESSAGE_ID_TYPE sgw_external_client::get_delivery_report_msg_id_base() const
{
    return delivery_report_msg_id_base_;
}

bool sgw_external_client::get_ignore_user_validity_period() const
{
    return ignore_user_validity_period_;
}

int sgw_external_client::get_max_submit_validity_period() const
{
    return max_submit_validity_period_;
}

int sgw_external_client::get_max_delivery_report_validity_period() const
{
    return max_delivery_report_validity_period_;
}

std::string sgw_external_client::get_system_id()
{
    return system_id_;
}

std::string sgw_external_client::get_system_type()
{
    return system_type_;
}

void sgw_external_client::set_submit_resp_msg_id_base(const std::shared_ptr<pa::config::node>& config)
{
    std::string submit_resp_msg_id_base_value = config->get<std::string>();

    if(("DEC" == submit_resp_msg_id_base_value) || ("dec" == submit_resp_msg_id_base_value))
    {
        submit_resp_msg_id_base_ = SMSC::Protobuf::DEC;
    }
    else if(("HEX" == submit_resp_msg_id_base_value) || ("hex" == submit_resp_msg_id_base_value))
    {
        submit_resp_msg_id_base_ = SMSC::Protobuf::HEX;
    }
    else
    {
        LOG_ERROR("This base type({}) is not valid", submit_resp_msg_id_base_value);
    }
}

void sgw_external_client::set_delivery_report_msg_id_base(const std::shared_ptr<pa::config::node>& config)
{
    std::string delivery_report_msg_id_base_value = config->get<std::string>();

    if(("DEC" == delivery_report_msg_id_base_value) || ("dec" == delivery_report_msg_id_base_value))
    {
        delivery_report_msg_id_base_ = SMSC::Protobuf::DEC;
    }
    else if(("HEX" == delivery_report_msg_id_base_value) || ("hex" == delivery_report_msg_id_base_value))
    {
        delivery_report_msg_id_base_ = SMSC::Protobuf::HEX;
    }
    else
    {
        LOG_ERROR("This base type({}) is not valid", delivery_report_msg_id_base_value);
    }
}

void sgw_external_client::set_system_type(const std::shared_ptr<pa::config::node>& config)
{
    system_type_ =  config->get<std::string>();
}

void sgw_external_client::set_require_password_checking(const std::shared_ptr<pa::config::node>& config)
{
    require_password_checking_ =  config->get<bool>();
}

void sgw_external_client::set_require_ip_checking(const std::shared_ptr<pa::config::node>& config)
{
    require_ip_checking_ =  config->get<bool>();
}

void sgw_external_client::set_ip_mask(const std::shared_ptr<pa::config::node>& config)
{
    ip_mask_ =  config->get<std::string>();
}

void sgw_external_client::set_ignore_user_validity_period(const std::shared_ptr<pa::config::node>& config)
{
    ignore_user_validity_period_ =  config->get<bool>();
}

void sgw_external_client::set_submit_validity_period(const std::shared_ptr<pa::config::node>& config)
{
    max_submit_validity_period_ =  config->get<int>();
}

void sgw_external_client::set_delivery_report_validity_period(const std::shared_ptr<pa::config::node>& config)
{
    max_delivery_report_validity_period_ =  config->get<int>();
}

void sgw_external_client::set_source_address_check(const std::shared_ptr<pa::config::node>& config)
{
    if(config->get<bool>())
        policy_commands_.insert(pa::paper::proto::Request::SOURCE_ADDRESS_CHECK);
    else
        policy_commands_.erase(pa::paper::proto::Request::SOURCE_ADDRESS_CHECK);
}

void sgw_external_client::set_source_ton_npi_check(const std::shared_ptr<pa::config::node>& config)
{
    if(config->get<bool>())
        policy_commands_.insert(pa::paper::proto::Request::SOURCE_TON_NPI_CHECK);
    else
        policy_commands_.erase(pa::paper::proto::Request::SOURCE_TON_NPI_CHECK);
}

void sgw_external_client::set_destination_address_check(const std::shared_ptr<pa::config::node>& config)
{
    if(config->get<bool>())
        policy_commands_.insert(pa::paper::proto::Request::DESTINATION_ADDRESS_CHECK);
    else
        policy_commands_.erase(pa::paper::proto::Request::DESTINATION_ADDRESS_CHECK);
}

void sgw_external_client::set_destination_ton_npi_check(const std::shared_ptr<pa::config::node>& config)
{
    if(config->get<bool>())
        policy_commands_.insert(pa::paper::proto::Request::DESTINATION_TON_NPI_CHECK);
    else
        policy_commands_.erase(pa::paper::proto::Request::DESTINATION_TON_NPI_CHECK);
}

void sgw_external_client::set_dcs_check(const std::shared_ptr<pa::config::node>& config)
{
    if(config->get<bool>())
        policy_commands_.insert(pa::paper::proto::Request::DCS_CHECK);
    else
        policy_commands_.erase(pa::paper::proto::Request::DCS_CHECK);
}

void sgw_external_client::set_black_white_check(const std::shared_ptr<pa::config::node>& config)
{
    if(config->get<bool>())
        policy_commands_.insert(pa::paper::proto::Request::BLACK_WHITE_CHECK);
    else
        policy_commands_.erase(pa::paper::proto::Request::BLACK_WHITE_CHECK);
}

void sgw_external_client::set_max_session(const std::shared_ptr<pa::config::node>& config)
{
    max_session_ =  config->get<int>();
}

void sgw_external_client::set_srr_state_generator(const std::shared_ptr<pa::config::node>& config)
{
    srr_state_generator_ = config->get<bool>();
}

void sgw_external_client::set_srr_state(const std::shared_ptr<pa::config::node>& config)
{
    srr_state_ = config->get<std::string>();
}
