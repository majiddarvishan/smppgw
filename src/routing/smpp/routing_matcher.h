#pragma once

#include "prefix_rule_matcher.h"

#include "src/sgw_definitions.h"
#include "src/libs/logging.hpp"

class routing_matcher
{
    class routing_info
    {
    public:
        routing_info(const std::shared_ptr<pa::config::node>& config)
        {
            //id_ = (std::hash<uint64_t>{}((uint64_t)config.get()));
            id_ = config->at("id")->get<uint32_t>();
            priority_ = config->at("priority")->get<uint32_t>();

            from_ = config->at("from")->get<std::string>();

            if(from_.empty())
            {
                from_ = "*";
            }

            transform(from_.begin(), from_.end(), from_.begin(), ::tolower);

            source_address_ = config->at("source_address")->get<std::string>();

            if(source_address_.empty())
            {
                source_address_ = "*";
            }

            destination_address_ = config->at("destination_address")->get<std::string>();

            if(destination_address_.empty())
            {
                destination_address_ = "*";
            }

            pdu_type_ = config->at("pdu_type")->get<std::string>();

            if(pdu_type_.empty())
            {
                pdu_type_ = "*";
            }

            if(pdu_type_ == "submit")
            {
                pdu_type_ = std::to_string(uint32_t(packet_type::ao));
            }
            else if(pdu_type_ == "deliver")
            {
                pdu_type_ = std::to_string(uint32_t(packet_type::at));
            }
            else if(pdu_type_ == "delivery_report")
            {
                pdu_type_ = std::to_string(uint32_t(packet_type::dr));
            }
            else if(pdu_type_ == "*")
            {
                pdu_type_ = "*";
            }
            else
            {
                LOG_ERROR("pdu_type {} is ionvalid !?", pdu_type_);
            }

            target_ = config->at("target")->get<std::string>();

            if(target_.empty())
            {
                target_ = "*";
            }
        }

        std::vector<std::vector<std::string> > get_list() const
        {
            return {
                { from_ }, { source_address_ }, { destination_address_ }, { pdu_type_ }
            };
        }

        std::string target() const
        {
            return target_;
        }

        uint64_t id() const
        {
            return id_;
        }

        inline uint16_t priority() const
        {
            return priority_;
        }

    private:
        uint64_t id_;
        uint16_t priority_ = 0;
        std::string target_;
        std::string from_ = "*";
        std::string source_address_ = "*";
        std::string destination_address_ = "*";
        std::string pdu_type_ = "*";
    };

public:
    routing_matcher(pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config);
    virtual ~routing_matcher() = default;

    std::string find_target(
        const std::string&                      from,
        const std::string&                      src_address,
        const std::string&                      dest_address,
        const std::string&                      pdu_type,
        std::function<bool(const std::string&)> is_available);

private:
    void init();

    bool add_rule(const routing_info& inputRule);
    bool remove_rule(uint64_t ruleId);

    void on_reverse_routing_replace(const std::shared_ptr<pa::config::node>& config);
    void on_routes_insert(const std::shared_ptr<pa::config::node>& config);
    void on_routes_remove(const std::shared_ptr<pa::config::node>& config);

    pa::config::manager* config_manager_;
    std::shared_ptr<pa::config::node> config_;

    pa::config::manager::observer config_obs_reverse_routing_replace_;
    pa::config::manager::observer config_obs_routes_insert_;
    pa::config::manager::observer config_obs_routes_remove_;

    bool reverse_routing_ = false;

    std::unique_ptr<prefix_rule_matcher> prefix_rule_matcher_;
    std::map<uint64_t, std::pair<std::string, uint16_t> > rule_id_mapping_;
    std::map<size_t, uint64_t> prm_id_to_rule_id_;
};
