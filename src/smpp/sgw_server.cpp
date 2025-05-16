#include "sgw_server.h"

#include "deliver_sm.h"

#include "src/routing/smpp/routing_matcher.h"
#include "src/logging/sgw_logger.h"

#include "tracer/MessageTracer.pb.h"

sgw_server::sgw_server(
    std::shared_ptr<smpp_gateway>            smpp_gateway,
    boost::asio::io_context*                 io_context,
    pa::config::manager*                     config_manager,
    const std::shared_ptr<pa::config::node>& config,
    const std::shared_ptr<pa::config::node>& prometheus_config,
    std::shared_ptr<prometheus::Registry> registry)
    : config_manager_(config_manager)
    , smpp_gateway_(smpp_gateway)
    , io_context_(io_context)
    , prometheus_config_(prometheus_config)
    , registry_(registry)
    , config_obs_external_client_insert_{config_manager->on_insert(config->at("external_client"), std::bind_front(&sgw_server::on_external_client_insert, this))}
    , config_obs_external_client_remove_{config_manager->on_remove(config->at("external_client"), std::bind_front(&sgw_server::on_external_client_remove, this))}
    , deliver_routing_family_counter_(prometheus::BuildCounter().Name("smpp_server_deliver_routing").Help("smpp server deliver routing parameters").Register(*registry))
    , dr_routing_family_counter_(prometheus::BuildCounter().Name("smpp_server_delivery_report_routing").Help("smpp server delivery_report parameters").Register(*registry))
    , smpp_server_bind_sysid_failed_(prometheus::BuildCounter().Name("smpp_server_bind_sysid_failed").Help("smpp server bind sysid failed").Register(*registry))
    , deliver_routing_failed_(add_counter(deliver_routing_family_counter_, prometheus_config->at("labels"), {
    { "name", "routing_failed" }
    }))
    , dr_routing_failed_(add_counter(dr_routing_family_counter_, prometheus_config->at("labels"), {
    { "name", "routing_failed" }
    }))
    , connection_reqs_sysid_failed_(add_counter(smpp_server_bind_sysid_failed_, prometheus_config->at("labels"), {
    { "name", "connection_reqs_sysid_failed" }
    }))
{
    ip_ = config->at("ip")->get<std::string>();
    port_ = config->at("port")->get<int>();
    system_id_ = config->at("system_id")->get<std::string>();
    timeout_ = config->at("session_init_timeout")->get<int>();
    inactivity_threshold_ = config->at("inactivity_timeout")->get<int>();
    enquirelink_threshold_ = config->at("enquire_link_timeout")->get<int>();

    for(const auto& ext_client_conf : config->at("external_client")->nodes())
    {
        std::string system_id = ext_client_conf->at("system_id")->get<std::string>();
        auto it = ext_clients_map_.find(system_id);

        if(it == ext_clients_map_.end())
        {
            auto ext_client = std::make_shared<sgw_external_client>(
                smpp_gateway_,
                io_context,
                config_manager_,
                ext_client_conf,
                prometheus_config_,
                registry_);

            ext_clients_map_.try_emplace(system_id, ext_client);
            LOG_DEBUG("client '{}' is inserted successfully.", system_id);
        }
        else
        {
            LOG_ERROR("client with system-id '{}' is exist.", system_id);
            throw std::runtime_error(fmt::format("client with system-id '{}' is already exist.", system_id));
        }
    }
    LOG_INFO("sgw_external_client configuration applied successfully.");

    routing_ = std::make_shared<routing_matcher>(config_manager_, config->at("routing"));

    LOG_INFO("sgw_server configuration applied successfully.");

    smpp_server_ = std::make_shared<pa::smpp::server>(
        io_context_,
        ip_,
        port_,
        system_id_,
        inactivity_threshold_,
        enquirelink_threshold_,
        std::bind_front(&sgw_server::on_authenticate_request, this),
        std::bind_front(&sgw_server::on_bind, this));

    smpp_server_->start();
}

sgw_server::~sgw_server()
{
}

// mshadow: todo: multi connection use different ip_address of unique system-id should be handle.(every connection can have specefic bind_type)
pa::smpp::command_status sgw_server::on_authenticate_request(const pa::smpp::bind_request& bind_request, const std::string& ip_address)
{
    LOG_DEBUG("authenticating ip_address:'{}' system_id:'{}'", ip_address, bind_request.system_id);

    std::shared_ptr<sgw_external_client> ext_client = get_external_client(bind_request.system_id);
    if(nullptr == ext_client)
    {
        LOG_ERROR("external-client with system id '{}' not exist", bind_request.system_id);
        connection_reqs_sysid_failed_.Increment();
        return pa::smpp::command_status::rinvsysid;
    }

    return ext_client->check_permision(bind_request.system_type, bind_request.password, ip_address, bind_request.bind_type);
}

void sgw_server::on_bind(const pa::smpp::bind_request& bind_request, std::shared_ptr<pa::smpp::session> session)
{
    LOG_DEBUG("new session has been binded system_id:'{}'", bind_request.system_id);

    auto ext_client = get_external_client(bind_request.system_id);

    if(nullptr == ext_client)
    {
        LOG_ERROR("session with system_id:'{}' not found.", bind_request.system_id);
        return;
    }

    ext_client->set_session(session);

    session->request_handler = std::bind_front(&sgw_external_client::on_session_request, ext_client);
    session->response_handler = std::bind_front(&sgw_external_client::on_session_response, ext_client);
    session->send_buf_available_handler = std::bind_front(&sgw_external_client::on_session_send_buf_available, ext_client);
    session->close_handler = std::bind_front(&sgw_external_client::on_session_close, ext_client);
    session->deserialization_error_handler = std::bind_front(&sgw_external_client::on_session_deserialization_error, ext_client);
}

void sgw_server::on_external_client_insert(const std::shared_ptr<pa::config::node>& config)
{
    std::string system_id = config->at("system_id")->get<std::string>();
    auto itr = ext_clients_map_.find(system_id);
    if(itr != ext_clients_map_.end())
    {
        LOG_ERROR("client '{}' is not inserted at runtime because its already exists.", system_id);
        throw std::runtime_error(fmt::format("client '{}' is not inserted at runtime because its already exists.", system_id));
    }
    else
    {
        auto ext_client = std::make_shared<sgw_external_client>(
            smpp_gateway_,
            io_context_,
            config_manager_,
            config,
            prometheus_config_,
            registry_
            );

        ext_clients_map_.try_emplace(system_id, ext_client);
        LOG_DEBUG("client '{}' is inserted successfully at runtime.", system_id);
    }
}

void sgw_server::on_external_client_remove(const std::shared_ptr<pa::config::node>& config)
{
    std::string system_id = config->at("system_id")->get<std::string>();
    auto itr = ext_clients_map_.find(system_id);
    if(itr != ext_clients_map_.end())
    {
        itr->second->stop();
        ext_clients_map_.erase(itr); //client removed
        LOG_DEBUG("client '{}' is removed successfully at runtime.", system_id);
        return;
    }

    LOG_ERROR("client '{}' is not removed at runtime.", system_id);
    throw std::runtime_error(fmt::format("client '{}' is not removed at runtime because its not exists.", system_id));
}

std::shared_ptr<sgw_external_client> sgw_server::get_external_client(const std::string& system_id)
{
    auto itr = std::find_if(ext_clients_map_.begin(), ext_clients_map_.end(), [&system_id](const auto& entry)
    {
        return entry.first == system_id;
    }
                            );

    if(itr != ext_clients_map_.end())
    {
        return (*itr).second; //return the shared_ptr to the client directly
    }
    else
    {
        return nullptr; //client not found
    }
}

std::string sgw_server::find_route(const std::string& from_id, const std::string& src_address, const std::string& dest_address, packet_type pdu_type)
{
    if(pdu_type == packet_type::at)
    {
        return routing_->find_target(from_id, src_address, dest_address, std::to_string(uint32_t(pdu_type)), std::bind_front(&sgw_server::is_available, this));
    }

    // these lines commented because smpp_gateway::find_route function called from deliver_sm only
    //if (pdu_type == packet_type::ao)
    //{
    //if (mnp_routing_enabled_)
    //{
    //auto itr = mnp_routing_map_.find(dest_address);

    //if (itr != mnp_routing_map_.end())
    //{
    //return itr->second;
    //}
    //}
    //return routing_->find_target(from_id, src_address, dest_address, std::to_string(uint32_t(pdu_type)), std::bind_front(&gateway::is_available_client, this));
    //}

    return "";
}

void sgw_server::send_deliver(std::shared_ptr<deliver_info> deliver_sm_info)
{
    auto sa = pa::smpp::convert_to_international((pa::smpp::ton)deliver_sm_info->request->mutable_smpp()->source_addr_ton(), deliver_sm_info->request->mutable_smpp()->source_addr());
    auto da = pa::smpp::convert_to_international((pa::smpp::ton)deliver_sm_info->request->mutable_smpp()->dest_addr_ton(), deliver_sm_info->request->mutable_smpp()->dest_addr());

    std::string client_id = find_route(deliver_sm_info->source_connection_, sa, da, packet_type::at);

    if (client_id.empty())
    {
        LOG_ERROR("could not find any route for received AT message {} {} {}", deliver_sm_info->source_connection_, sa, da);
        deliver_sm::process_resp(smpp_gateway_, deliver_sm_info, pa::smpp::command_status::rinvdstadr);
        deliver_routing_failed_.Increment();
        return;
    }

    deliver_sm_info->dest_connection_ = client_id;

    auto ext_client = get_external_client(client_id);

    if (nullptr == ext_client)
    {
        LOG_ERROR("destination client with id '{}' not found!", client_id);
        deliver_sm::process_resp(smpp_gateway_, deliver_sm_info, pa::smpp::command_status::rinvdstadr);
        deliver_routing_failed_.Increment();
        return;
    }

    ext_client->flow_controlled_send_deliver(deliver_sm_info);
}

void sgw_server::send_delivery_report(const std::string& orig_cp_id, std::shared_ptr<deliver_info> deliver_info)
{
    deliver_info->dest_connection_ = orig_cp_id;

    auto ext_client = get_external_client(orig_cp_id);

    if (nullptr == ext_client)
    {
        LOG_ERROR("destination client with id '{}' not found!", orig_cp_id);
        deliver_info->error_ = pa::smpp::command_status::rinvdstadr;
        deliver_sm::process_resp(smpp_gateway_, deliver_info, pa::smpp::command_status::rinvdstadr);
        dr_routing_failed_.Increment();
        return;
    }

    ext_client->send_deliver_sm(deliver_info);
}

//TODO: Majid Darvishan, should be implemented
bool sgw_server::is_available(const std::string& id)
{
    return true;
}
