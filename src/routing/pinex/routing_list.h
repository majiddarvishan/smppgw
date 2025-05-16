#pragma once

#include "route_mapper.h"

#include <pa/config.hpp>

#include <map>
#include <memory>

enum class routing_method
{
    RM_ROUND_ROBIN        = 0,
    RM_LOAD_BALANCE       = 1,
    RM_BROAD_CAST         = 2,
    RM_ROUTE_BY_CLIENT_ID = 3,
    RM_ROUTE_BY_PREFIX    = 4
};

using msg_type_convert_handler_t = std::function<int(const std::string&)>;

class route
{
public:
    route(const std::shared_ptr<pa::config::node>& config, msg_type_convert_handler_t handler);
    virtual ~route() = default;

public:
    int msg_type_;
    routing_method method_;
    std::string target_;

    route_mapper<std::string> destinations_;
};

class routing_list
{
public:
    routing_list(pa::config::manager* config_manager,
                const std::shared_ptr<pa::config::node>& config,
               msg_type_convert_handler_t handler);

    ~routing_list() = default;

    void on_routes_remove(const std::shared_ptr<pa::config::node>& config);
    void on_routes_insert(const std::shared_ptr<pa::config::node>& config);

    routing_method find_routing_method(int msgType);

    std::string find_target(std::string prefix, int msgType, bool reverse = false);

    msg_type_convert_handler_t msg_type_convert_handler_;

    pa::config::manager::observer config_obs_routes_insert_;
    pa::config::manager::observer config_obs_routes_remove_;

public:
    std::map<int, std::shared_ptr<route> > msg_type_to_route_map_;
};
