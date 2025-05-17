#pragma once

#include "paper/command.pb.h"
#include "src/libs/monitoring.hpp"

#include <pinex/p_client_list.hpp>
#include <smpp/smpp.hpp>

#include <boost/asio/io_context.hpp>
#include <prometheus/registry.h>

#include <functional>
#include <memory>

class submit_info;
class smpp_gateway;

class paper_client
{
public:
    explicit paper_client(
        std::shared_ptr<smpp_gateway>,
        boost::asio::io_context*,
        pa::config::manager*,
        const std::shared_ptr<pa::config::node>&,
        const std::shared_ptr<pa::config::node>&,
        std::shared_ptr<prometheus::Registry> registry);

    paper_client(const paper_client&) = delete;
    virtual ~paper_client() = default;

    void start();
    void stop();

    bool check_policies(const std::string&                                       rcv_clnt_sys_id,
                        std::shared_ptr<submit_info>                             request,
                        const std::set<pa::paper::proto::Request_Type>& commands);

private:
    void receive_response(const std::string& client_id, uint32_t seq_no, const std::string& msg_body);
    void timeout_request(const std::string& client_id, uint32_t seq_no, const std::string& msg_body);
    void session_state_changed(const std::string& name, session_stat state);

    void process_resp(const std::string& client_id, uint32_t seq_no, pa::paper::proto::Response&& resp);

    void on_timeout_replace(const std::shared_ptr<pa::config::node>& config);

    std::shared_ptr<smpp_gateway> smpp_gateway_;

    boost::asio::io_context* io_context_;
    pa::config::manager* config_manager_;
    std::shared_ptr<pa::config::node> config_;
    std::shared_ptr<pa::config::node> prometheus_config_;

    std::atomic<bool> run_;

    std::vector<std::string> trasnports_;
    std::string client_name_;
    uint32_t timeout_;

    pa::config::manager::observer config_obs_timeout_replace_;

    prometheus::Family<prometheus::Gauge>& policy_connection_family_gauge_;
    prometheus::Family<prometheus::Counter>& policy_rules_family_counter_;

    prometheus::Gauge& connected_clients_;
    prometheus::Counter& req_success_;
    prometheus::Counter& req_failed_;
    prometheus::Counter& resp_received_;
    prometheus::Counter& resp_timeouts_;
    prometheus::Counter& black_white_check_;
    prometheus::Counter& source_address_check_;
    prometheus::Counter& source_ton_npi_check_;
    prometheus::Counter& destination_address_check_;
    prometheus::Counter& destination_ton_npi_check_;
    prometheus::Counter& dcs_check_;
    prometheus::Counter& submits_rejected_;

    std::shared_ptr<pa::pinex::p_client_list> client_list_;

    std::unordered_map<std::string, std::shared_ptr<submit_info> > user_data_;
};
