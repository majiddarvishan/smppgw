#include "src/paper/paper_client.h"
#include "src/smpp/sgw_external_client.h"
#include "src/libs/logging.hpp"

static std::map<pa::paper::proto::Status, pa::smpp::command_status> paper_status_to_smpp = {
    { pa::paper::proto::Status::OK,                               pa::smpp::command_status::rok                   },
    { pa::paper::proto::Status::TIMEOUT,                          pa::smpp::command_status::rtimeout              },
    { pa::paper::proto::Status::FORMAT_ERROR,                     pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::NO_CREDIT,                        pa::smpp::command_status::src_has_no_credit     },
    { pa::paper::proto::Status::INVALID_DATA_CODING,              pa::smpp::command_status::rinvdcs               },
    { pa::paper::proto::Status::SOURCE_NUMBER_IN_BLACK_LIST,      pa::smpp::command_status::src_in_black_list     },
    { pa::paper::proto::Status::SOURCE_NUMBER_NOT_IN_WHITE_LIST,  pa::smpp::command_status::src_not_in_white_list },
    { pa::paper::proto::Status::SOURCE_NUMBER_NOT_VALID,          pa::smpp::command_status::rinvsrcadr            },
    { pa::paper::proto::Status::SOURCE_NUMBER_INVALID_TON,        pa::smpp::command_status::rinvsrcton            },
    { pa::paper::proto::Status::SOURCE_NUMBER_INVALID_NPI,        pa::smpp::command_status::rinvsrcnpi            },
    { pa::paper::proto::Status::DESTINATION_NUMBER_NOT_VALID,     pa::smpp::command_status::rinvdstadr            },
    { pa::paper::proto::Status::DESTINATION_NUMBER_INVALID_TON,   pa::smpp::command_status::rinvdstton            },
    { pa::paper::proto::Status::DESTINATION_NUMBER_INVALID_NPI,   pa::smpp::command_status::rinvdstnpi            },
    { pa::paper::proto::Status::DESTINATION_NUMBER_IN_BLACK_LIST, pa::smpp::command_status::dst_in_black_list     },
    { pa::paper::proto::Status::LIF_NO_FILTER,                    pa::smpp::command_status::rok                   },
    { pa::paper::proto::Status::LIF_SUBSCRIBER_WHITELIST,         pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::LIF_OPERATOR_WHITELIST,           pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::LIF_RESTRICTED_LIST,              pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::LIF_BINARY,                       pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::LIF_RULE,                         pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::LIF_LOCATION_MISSING,             pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::FILTERING_RULES_SUCCESS,          pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::FILTERING_RULES_INTERNAL_ERROR,   pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::FILTERING_RULES_BAD_PACKET,       pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::FILTERING_RULES_ROW_NOT_EXIST,    pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::CREDIT_CONTROL_FAILED,            pa::smpp::command_status::src_has_no_credit     },
    { pa::paper::proto::Status::REFUND_FAILED,                    pa::smpp::command_status::rsyserr               },
    { pa::paper::proto::Status::SPOOFED_NUMBER,                   pa::smpp::command_status::rinvsrcadr            },
    { pa::paper::proto::Status::UNSUPPORTED_COMMAND,              pa::smpp::command_status::rsyserr               }
};

paper_client::paper_client(
    std::shared_ptr<smpp_gateway>            smpp_gateway,
    boost::asio::io_context*                 io_context,
    pa::config::manager*                     config_manager,
    const std::shared_ptr<pa::config::node>& config,
    const std::shared_ptr<pa::config::node>& prometheus_config,
    std::shared_ptr<prometheus::Registry> registry)
    : smpp_gateway_{smpp_gateway}
    , io_context_{io_context}
    , config_manager_{config_manager}
    , config_{config}
    , prometheus_config_{prometheus_config}
    , run_{false}
    , client_name_ { config_->at("name")->get<std::string>() }
    , timeout_ { config_->at("timeout")->get<uint32_t>() }
    , config_obs_timeout_replace_{config_manager->on_replace(config->at("timeout"), std::bind_front(&paper_client::on_timeout_replace, this))}
    , policy_connection_family_gauge_{prometheus::BuildGauge().Name("policy_connection").Help("policy parameters").Register(*registry)}
    , policy_rules_family_counter_{prometheus::BuildCounter().Name("policy_rules").Help("policy parameters").Register(*registry)}
    , connected_clients_(add_gauge(policy_connection_family_gauge_, prometheus_config_->at("labels"), {
    { "name", "connected_clients" }, { "category", "network" }, { "system_id", config->at("name")->get<std::string>() }
}))
    , req_success_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "req_succeed" }, { "category", "network" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , req_failed_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "req_failed" }, { "category", "network" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , resp_received_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "resp_received" }, { "category", "network" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , resp_timeouts_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "resp_timeouts" }, { "category", "network" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , black_white_check_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "black_white_check" }, { "category", "command" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , source_address_check_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "source_address_check" }, { "category", "command" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , source_ton_npi_check_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "source_ton_npi_check" }, { "category", "command" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , destination_address_check_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "destination_address_check" }, { "category", "command" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , destination_ton_npi_check_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "destination_ton_npi_check" }, { "category", "command" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , dcs_check_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
    { "name", "dcs_check" }, { "category", "command" }, { "system_id", config->at("name")->get<std::string>()}
}))
    , submits_rejected_(add_counter(policy_rules_family_counter_, prometheus_config->at("labels"), {
        { "name", "submits_rejected" }, { "system_id", config->at("name")->get<std::string>() }
}))
{
    LOG_INFO("paper_client configuration applied successfully.");

    for(auto& n : config_->at("address")->nodes())
        trasnports_.push_back(n->get<std::string>());
}

void paper_client::start()
{
    srand(time(0));

    if(client_list_)
    {
        return;
    }

    client_list_ = std::make_shared<pa::pinex::p_client_list>(
        io_context_,
        client_name_,
        trasnports_,
        timeout_,
        true, /*auto_reconnect*/
        nullptr,
        std::bind_front(&paper_client::receive_response, this),
        std::bind_front(&paper_client::timeout_request, this),
        std::bind_front(&paper_client::session_state_changed, this));
} //paper_client::start

void paper_client::stop()
{
    if(!client_list_)
    {
        return;
    }

    client_list_->stop();
    client_list_ = nullptr;
}

void paper_client::session_state_changed(const std::string& name, session_stat state)
{
    switch(state)
    {
        case session_stat::open:
            LOG_INFO("State of connection {} => {} is changed to open", client_name_, name);
            connected_clients_.Decrement();
            break;

        case session_stat::bind:
            LOG_INFO("State of connection {} => {} is changed to bind", client_name_, name);
            connected_clients_.Increment();
            break;

        case session_stat::close:
            LOG_INFO("State of connection {} => {} is changed to close", client_name_, name);
            connected_clients_.Decrement();
            break;
    } //switch
}

void paper_client::on_timeout_replace(const std::shared_ptr<pa::config::node>& config)
{
    timeout_ = config->get<int8_t>();
    //todo majid: implement client_list_ timeout update runtime
}

// mshadow: todo: should be refactor because perform multitasks and does not follow solid principles. it should check policy not send submit_sm packet
bool paper_client::check_policies(const std::string&                                       rcv_clnt_sys_id,
                                  std::shared_ptr<submit_info>                             user_data,
                                  const std::set<pa::paper::proto::Request_Type>& commands)
{
    if(!this->client_list_)
    {
        req_failed_.Increment();
        return false;
    }

    pa::paper::proto::Request cmd;

    // cmd.set_msg_id(rand() % INT_MAX);

    cmd.set_cp_system_id(rcv_clnt_sys_id);

    cmd.set_src_address(pa::smpp::convert_to_international(user_data->request.source_addr_ton, user_data->request.source_addr));
    cmd.set_src_ton(static_cast<uint32_t>(user_data->request.dest_addr_ton));
    cmd.set_src_npi(static_cast<uint32_t>(user_data->request.source_addr_npi));

    cmd.set_dst_address(pa::smpp::convert_to_international(user_data->request.dest_addr_ton, user_data->request.dest_addr));
    cmd.set_dst_ton(static_cast<uint32_t>(user_data->request.dest_addr_ton));
    cmd.set_dst_npi(static_cast<uint32_t>(user_data->request.dest_addr_ton));

    cmd.set_dcs(static_cast<uint32_t>(user_data->request.data_coding));

    for(auto itr : commands)
    {
        switch(itr)
        {
            case pa::paper::proto::Request::SOURCE_ADDRESS_CHECK:
                source_address_check_.Increment();
                cmd.add_cmd_type(pa::paper::proto::Request::SOURCE_ADDRESS_CHECK);
                break;

            case pa::paper::proto::Request::SOURCE_TON_NPI_CHECK:
                source_ton_npi_check_.Increment();
                cmd.add_cmd_type(pa::paper::proto::Request::SOURCE_TON_NPI_CHECK);
                break;

            case pa::paper::proto::Request::DESTINATION_ADDRESS_CHECK:
                destination_address_check_.Increment();
                cmd.add_cmd_type(pa::paper::proto::Request::DESTINATION_ADDRESS_CHECK);
                break;

            case pa::paper::proto::Request::DESTINATION_TON_NPI_CHECK:
                destination_ton_npi_check_.Increment();
                cmd.add_cmd_type(pa::paper::proto::Request::DESTINATION_TON_NPI_CHECK);
                break;

            case pa::paper::proto::Request::DCS_CHECK:
                dcs_check_.Increment();
                cmd.add_cmd_type(pa::paper::proto::Request::DCS_CHECK);
                break;

            case pa::paper::proto::Request::BLACK_WHITE_CHECK:
                black_white_check_.Increment();
                cmd.add_cmd_type(pa::paper::proto::Request::BLACK_WHITE_CHECK);
                break;

            default:
                LOG_DEBUG("This command ({}) is not supported yet", static_cast<int>(itr));
        } //switch
    }

    log_protobuf_message(cmd);

    //TODO:: check
    std::string message = cmd.SerializeAsString();

    try
    {
        auto [seq_no, id] = client_list_->send_request(message);

        if(seq_no)
        {
            LOG_DEBUG("Send command to paper");
            user_data_.insert({ fmt::format("{}@{}", seq_no, id), user_data });

            req_success_.Increment();
            return true;
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

    LOG_ERROR("Could not send command to paper");

    req_failed_.Increment();
    return false;
} //paper_client::check_policies

void paper_client::receive_response(const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    LOG_DEBUG("Receive response with sequence: '{}' from paper: '{}'", seq_no, client_id);

    resp_received_.Increment();
    LOG_HEX(spdlog::level::info, msg_body);

    pa::paper::proto::Response resp;

    if(!resp.ParseFromString(msg_body))
    {
        LOG_ERROR("Unsupported resp format!");
        return;
    }

    log_protobuf_message(resp);

    paper_client::process_resp(client_id, seq_no, std::move(resp));
} //paper_client::receive_response

void paper_client::timeout_request(const std::string& client_id, uint32_t seq_no, const std::string&)
{
    LOG_DEBUG("timeout request with sequence: {{}} from paper: {{}}", seq_no, client_id);

    resp_timeouts_.Increment();

    pa::paper::proto::Response resp;
    resp.set_status(pa::paper::proto::TIMEOUT);

    log_protobuf_message(resp);

    paper_client::process_resp(client_id, seq_no, std::move(resp));
}

void paper_client::process_resp(const std::string& client_id, uint32_t seq_no, pa::paper::proto::Response&& resp)
{
    auto it = user_data_.find(fmt::format("{}@{}", seq_no, client_id));

    if(it == user_data_.end())
    {
        LOG_ERROR("Could not find user_data for sequence {}", seq_no);
        return;
    }

    auto submit_data = it->second;
    user_data_.erase(it);

    //todo:majid darvishan => what we can do here?
    //try
    //{
    ////commands_monitor_.status_counter_.at(resp.status())++;
    //}
    //catch(std::out_of_range& exp)
    //{
    //LOG_DEBUG("Why get error {} from PAPER, exception: {}", (uint32_t)resp.status(), exp.what());
    ////commands_monitor_.status_counter_.insert(std::make_pair(resp.status(), MonitoringVariable("Policy.Status." + std::to_string(resp.status()))));
    //}

    try
    {
        submit_data->error_ = paper_status_to_smpp.at(resp.status());
    }
    catch(std::out_of_range& exp)
    {
        LOG_DEBUG("Why get error {} from PAPER, exception: {}", static_cast<int>(resp.status()), exp.what());
        submit_data->error_ = pa::smpp::command_status::rsyserr;
    }

    if(submit_data->error_ != pa::smpp::command_status::rok)
        submits_rejected_.Increment();

    submit_sm::on_check_policies_responce(this->smpp_gateway_, submit_data);
}

