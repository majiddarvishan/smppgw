#pragma once

#include "routing_list.h"

#include <pa/config.hpp>

class router
{
public:
    router(pa::config::manager* config_manager,
    const std::shared_ptr<pa::config::node>& config,
    msg_type_convert_handler_t handler);

    virtual ~router();

    void add_target(const std::string& target);
    void remove_target(const std::string& target);

    void find(int msg_type, const std::string& id, std::vector<std::string>& targets_list);

    void on_reverse_replace(const std::shared_ptr<pa::config::node>& config);

protected:
    void route_by_broad_casting(std::vector<std::string>& destinationAddresses);
    void route_by_client_id(const std::string& clientId, std::vector<std::string>& destinationAddresses);
    void route_by_prefix(const std::string& prefix, int msg_type, std::vector<std::string>& destinationAddresses, bool reverse);
    void route_by_round_robin(std::vector<std::string>& destinationAddresses);
    void route_by_load_balance(const std::string& prefix, std::vector<std::string>& destinationAddresses);

    int last_index_;
    bool reverse_;
    std::shared_ptr<routing_list> routingList_;
    std::vector<std::string> targets_list_;

    pa::config::manager::observer config_obs_reverse_replace_;
};
