#include "router.h"

#include "src/libs/logging.hpp"

#include <algorithm>

router::router(pa::config::manager* config_manager,
               const std::shared_ptr<pa::config::node>& config,
               msg_type_convert_handler_t handler)
    : last_index_{0}
    , reverse_ {config->at("reverse")->get<bool>()}
    , routingList_ {std::make_shared<routing_list>(config_manager, config->at("routing_list"), handler)}
    , config_obs_reverse_replace_{config_manager->on_replace(config->at("reverse"), std::bind_front(&router::on_reverse_replace, this))}
{
    LOG_INFO("router configuration applied successfully.");
}

router::~router()
{
}

void router::on_reverse_replace(const std::shared_ptr<pa::config::node>& config)
{
    reverse_ = config->get<bool>();
}

void router::add_target(const std::string& target)
{
    if(std::none_of(targets_list_.begin(), targets_list_.end(), [&target](std::string& name) {
        return target == name;
    }))
    {
        targets_list_.push_back(target);

        std::sort(targets_list_.begin(), targets_list_.end());
    }
    else
    {
        LOG_ERROR("target {} is already exist", target);
    }
}

void router::remove_target(const std::string& target)
{
    for(unsigned i = 0; i < targets_list_.size(); i++)
    {
        if(targets_list_[i] == target)
        {
            targets_list_.erase(targets_list_.begin() + i);
        }
    }

    last_index_ = 0;
} //router::remove_connection

void router::find(int msg_type, const std::string& id, std::vector<std::string>& targets_list)
{
    auto method = routingList_->find_routing_method(msg_type);

    switch(method)
    {
        case routing_method::RM_BROAD_CAST:
            route_by_broad_casting(targets_list);
            break;

        case routing_method::RM_LOAD_BALANCE:
            route_by_load_balance(id, targets_list);
            break;

        case routing_method::RM_ROUND_ROBIN:
            route_by_round_robin(targets_list);
            break;

        case routing_method::RM_ROUTE_BY_PREFIX:
            route_by_prefix(id, msg_type, targets_list, reverse_);
            break;

        case routing_method::RM_ROUTE_BY_CLIENT_ID:
            route_by_client_id(id, targets_list);
            break;

        default:
            break;
    } //switch
} //router::select_destination_addresses

void router::route_by_broad_casting(std::vector<std::string>& targets_list)
{
    for(std::string& item : targets_list_)
    {
        targets_list.push_back(item);
    }
}

void router::route_by_client_id(const std::string& clientId, std::vector<std::string>& targets_list)
{
    if(clientId.size())
    {
        targets_list.push_back(clientId);
    }
}

void router::route_by_prefix(const std::string& prefix, int msg_type, std::vector<std::string>& targets_list, bool reverse)
{
    auto r = routingList_->find_target(prefix, msg_type, reverse);

    if(r.size())
    {
        targets_list.push_back(r);
    }
}

void router::route_by_round_robin(std::vector<std::string>& targets_list)
{
    if(targets_list_.size() > 0)
    {
        targets_list.push_back(targets_list_[last_index_]);
        last_index_ = (last_index_ + 1) % targets_list_.size();
    }
}

struct MyHash
{
    int operator()(const std::string& str) const noexcept
    {
        return std::stoi(str.substr(str.length() - 5));
    }
};

void router::route_by_load_balance(const std::string& prefix, std::vector<std::string>& targets_list)
{
    if(targets_list_.size() > 0)
    {
        int h = MyHash{}(prefix);
        int index = h % targets_list_.size();
        targets_list.push_back(targets_list_[index]);
    }
}
