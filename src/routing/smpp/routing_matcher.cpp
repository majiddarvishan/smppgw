#include "routing_matcher.h"

#include <memory>

constexpr static size_t ROUTING_FIELD_NUMBER = 4;
constexpr static size_t MAXIMUM_RULE_NUMBER = 1000;

routing_matcher::routing_matcher(pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config)
    : config_manager_{config_manager}
    , config_{config}
    , config_obs_reverse_routing_replace_{config_manager->on_replace(config->at("reverse"), std::bind_front(&routing_matcher::on_reverse_routing_replace, this))}
    , config_obs_routes_insert_{config_manager->on_insert(config->at("routes"), std::bind_front(&routing_matcher::on_routes_insert, this))}
    , config_obs_routes_remove_{config_manager->on_remove(config->at("routes"), std::bind_front(&routing_matcher::on_routes_remove, this))}
{
    prefix_rule_matcher_ = std::make_unique<prefix_rule_matcher>(MAXIMUM_RULE_NUMBER, ROUTING_FIELD_NUMBER);

    init();
}

void routing_matcher::init()
{
    reverse_routing_ = config_->at("reverse")->get<bool>();

    for(const auto& route_config : config_->at("routes")->nodes())
    {
        routing_info route = routing_info(route_config);
        add_rule(route);
    }
}

std::string routing_matcher::find_target(
    const std::string&                      from,
    const std::string&                      src_address,
    const std::string&                      dest_address,
    const std::string&                      pdu_type,
    std::function<bool(const std::string&)> is_available)
{
    auto dest = dest_address;

    if(reverse_routing_)
    {
        std::reverse(dest.begin(), dest.end());
    }

    auto f = from;
    transform(f.begin(), f.end(), f.begin(), ::tolower);

    auto data = { f, src_address, dest, pdu_type };

    std::vector<std::pair<std::string, uint16_t> > targetMatchList;
    const std::vector<uint32_t> matchList = prefix_rule_matcher_->match(data);

    for(const auto& matchId : matchList)
    {
        const auto& prmRuleIdIter = prm_id_to_rule_id_.find(matchId);

        if(prmRuleIdIter != prm_id_to_rule_id_.end())
        {
            const auto& ruleIdIter = rule_id_mapping_.find(prmRuleIdIter->second);

            if(ruleIdIter != rule_id_mapping_.end())
            {
                targetMatchList.push_back(ruleIdIter->second);
            }
        }
    }

    std::string matched;
    int priority = -1;

    for(auto& itr : targetMatchList)
    {
        if(itr.second > priority)
        {
            matched = itr.first;
        }
    }

    return matched;
} //routing_matcher::find_target

bool routing_matcher::add_rule(const routing_info& inputRule)
{
    if(rule_id_mapping_.find(inputRule.id()) != rule_id_mapping_.end())
    {
        return false;
    }

    try
    {
        const size_t pRMId = prefix_rule_matcher_->set_rule(inputRule.get_list());
        prm_id_to_rule_id_[pRMId] = inputRule.id();
        rule_id_mapping_.emplace(inputRule.id(), std::make_pair(inputRule.target(), inputRule.priority()));
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("%s", e.what());
        return false;
    }

    return true;
} //routing_matcher::add_rule

bool routing_matcher::remove_rule(uint64_t ruleId)
{
    const auto iter = rule_id_mapping_.find(ruleId);

    if(iter != rule_id_mapping_.end())
    {
        rule_id_mapping_.erase(iter);

        auto it = prm_id_to_rule_id_.begin();

        while(it != prm_id_to_rule_id_.end())
        {
            if(it->second == ruleId)
            {
                it = prm_id_to_rule_id_.erase(it);
                break;
            }
            else
            {
                it++;
            }
        }

        return true;
    }

    return false;
} //routing_matcher::remove_rule

void routing_matcher::on_reverse_routing_replace(const std::shared_ptr<pa::config::node>& config)
{
    reverse_routing_ = config->get<bool>();
}

void routing_matcher::on_routes_insert(const std::shared_ptr<pa::config::node>& config)
{
    routing_info route = routing_info(config);

    if(!add_rule(route))
    {
        throw std::runtime_error("could not add route");
    }
}

void routing_matcher::on_routes_remove(const std::shared_ptr<pa::config::node>& config)
{
    auto id = config->at("id")->get<uint32_t>();

    if(!remove_rule(id))
    {
        throw std::runtime_error("could not remove route");
    }
}
