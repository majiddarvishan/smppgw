#include "routing_list.h"

#include "src/libs/logging.hpp"

#include <boost/algorithm/string.hpp>
#include <algorithm>

static routing_method create_method_by_string(const std::string& methodStr)
{
    if(methodStr == "rond_robin")
    {
        return routing_method::RM_ROUND_ROBIN;
    }

    if(methodStr == "load_balance")
    {
        return routing_method::RM_LOAD_BALANCE;
    }

    if(methodStr == "broadcast")
    {
        return routing_method::RM_BROAD_CAST;
    }

    if(methodStr == "prefix")
    {
        return routing_method::RM_ROUTE_BY_PREFIX;
    }

    if(methodStr == "client_id")
    {
        return routing_method::RM_ROUTE_BY_CLIENT_ID;
    }

    LOG_ERROR("invalid routing method!");
    throw std::runtime_error("invalid routing method!");
}

/***********************************************************************************************/
/*************************************  Route **************************************************/
/***********************************************************************************************/

route::route(const std::shared_ptr<pa::config::node>& config,
               msg_type_convert_handler_t handler)
{
    msg_type_ = handler(config->at("msg_type")->get<std::string>());
    method_ = create_method_by_string(config->at("method")->get<std::string>());

    if(method_ == routing_method::RM_ROUTE_BY_PREFIX)
    {
        for(const auto& r : config->at("routes")->nodes())
        {
            auto id = r->at("id")->get<std::string>();
            target_ = r->at("target")->get<std::string>();

            for(const auto& prefix : r->at("prefix")->nodes())
            {
                auto p = prefix->get<std::string>();
                destinations_.add_route(p, target_);
                // prefixes_.push_back(p);
            }

        }
    }
}

/***********************************************************************************************/
/*************************************  RoutingList  *******************************************/
/***********************************************************************************************/

routing_list::routing_list(pa::config::manager* config_manager,
                const std::shared_ptr<pa::config::node>& config,
                msg_type_convert_handler_t handler)
    : msg_type_convert_handler_{handler}
    , config_obs_routes_insert_{config_manager->on_insert(config, std::bind_front(&routing_list::on_routes_insert, this))}
    , config_obs_routes_remove_{config_manager->on_remove(config, std::bind_front(&routing_list::on_routes_remove, this))}
{
    for(const auto& route_config : config->nodes())
    {
        auto r = std::make_shared<route>(route_config, handler);
        auto [it, success] = msg_type_to_route_map_.insert({ r->msg_type_, r });

        if(!success)
        {
            LOG_ERROR("route " + route_config->at("msg_type")->get<std::string>() + " is duplicated!");
            throw std::runtime_error("route " + route_config->at("msg_type")->get<std::string>() + " is duplicated!");
        }

        // for(auto& p : r->prefixes_)
        // {
        //     destinations_.add_route(p, r->target_);
        // }
    }
}

void routing_list::on_routes_insert(const std::shared_ptr<pa::config::node>& config)
{
    std::shared_ptr<route> routes = std::make_shared<route>(config, msg_type_convert_handler_);

    auto [it, success] = msg_type_to_route_map_.insert({ routes->msg_type_, routes });

    if(!success)
    {
        LOG_ERROR("route " + config->at("msg_type")->get<std::string>() + " is duplicated!");
        throw std::runtime_error("route " + config->at("msg_type")->get<std::string>() + " is duplicated!");
    }

    // for(auto& p : routes->prefixes_)
    // {
    //     destinations_.add_route(p, routes->target_);
    // }
}

void routing_list::on_routes_remove(const std::shared_ptr<pa::config::node>& config)
{
    std::shared_ptr<route> routes = std::make_shared<route>(config, msg_type_convert_handler_);
    msg_type_to_route_map_.erase(routes->msg_type_);

    // for(auto& p : routes->prefixes_)
    // {
    //     destinations_.delete_route(p);
    // }
}

routing_method routing_list::find_routing_method(int msgType)
{
    routing_method method = routing_method::RM_ROUND_ROBIN;
    auto itr = msg_type_to_route_map_.find(msgType);

    if(itr != msg_type_to_route_map_.end())
    {
        method = itr->second->method_;
    }

    return method;
}

std::string routing_list::find_target(std::string prefix, int msgType, bool reverse)
{
    auto itr = msg_type_to_route_map_.find(msgType);

    if(itr != msg_type_to_route_map_.end())
    {
        return itr->second->destinations_.find_destination(prefix, reverse);
    }

    return "";
}
