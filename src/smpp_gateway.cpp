#include "src/logging/sgw_logger.h"
#include "src/routing/smpp/routing_matcher.h"
#include "src/smpp/sgw_server.h"
#include "src/paper/paper_client.h"

//mshadow: todo: It is better to use a callback functions instead of passing the SMPP object to other class
smpp_gateway::smpp_gateway(boost::asio::io_context* io_context, pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config)
    : io_context_(io_context)
    , config_manager_(config_manager)
    , config_(config)
    , timer_(*io_context_)
    , run_(false)
    // , reverse_routing_ { config->at("routing")->at("reverse_routing")->get<bool>() }
    // , smpp_server_ { std::make_shared<sgw_server>(
    //     shared_from_this(),
    //     io_context_,
    //     config_manager_,
    //     config_->at("smpp_server"),
    //     config_->at("prometheus"),
    //     registry_
    //     ) }
    // , pinex_ {std::make_shared<pinex>(
    //     shared_from_this(),
    //     io_context_,
    //     config_manager_,
    //     config_->at("network_interface"),
    //     config_->at("prometheus"),
    //     registry_
    //     )}
    // , paper_client_ { std::make_shared<paper_client>(
    //     shared_from_this(),
    //     io_context_,
    //     config_manager_,
    //     config_->at("policy"),
    //     config_->at("prometheus"),
    //     registry_
    //     )}
    // , routing_ {std::make_shared<routing_matcher>(config_manager_, config_->at("routing"))}
    , exposer_(std::make_unique<prometheus::Exposer>(config_->at("prometheus")->at("address")->get<std::string>()))
    , registry_(std::make_shared<prometheus::Registry>())
{

}

void smpp_gateway::initilize()
{
    smpp_server_ = std::make_shared<sgw_server>(
        shared_from_this(),
        io_context_,
        config_manager_,
        config_->at("smpp_server"),
        config_->at("prometheus"),
        registry_
    );

    pinex_ = std::make_shared<pinex>(
        shared_from_this(),
        io_context_,
        config_manager_,
        config_->at("network_interface"),
        config_->at("prometheus"),
        registry_
    );

    paper_client_ = std::make_shared<paper_client>(
        shared_from_this(),
        io_context_,
        config_manager_,
        config_->at("policy"),
        config_->at("prometheus"),
        registry_
        );

    logger_ = sgw_logger::getInstance();
    logger_->SetSmppGateway(shared_from_this());
    logger_->load_config(io_context_, config_manager_, config_->at("logger"));
}

void smpp_gateway::start()
{
    paper_client_->start();

    if(pinex_)
    {
        pinex_->start();
    }

    start_time_ = time(nullptr);
    run_.store(true);
    exposer_->RegisterCollectable(registry_);
    do_set_timer();
}

void smpp_gateway::do_set_timer()
{
    timer_.expires_after(std::chrono::seconds(1));
    timer_.async_wait([this, wptr = weak_from_this()](std::error_code ec) {
        if(!wptr.expired())
        {
            if(ec)
            {
                throw std::runtime_error("io::expirator::async_wait() " + ec.message());
            }
            do_set_timer();
        }
    });
}

bool smpp_gateway::is_run() const
{
    return run_.load();
}

void smpp_gateway::stop()
{
    smpp_server_.reset();
    //mRun.store(false);
    //
    //mSmppServer->Stop();
    //
    //paper_client_->Stop();
    //
    //while(!mSmppServer->IsStop())
    //{
    //sleep(1);
    //}
    //
    //SGW_Logger::getInstance()->Close();
}

bool smpp_gateway::check_policies(const std::string&                             rcv_clnt_sys_id,
                        std::shared_ptr<submit_info>                             request,
                        const std::set<pa::paper::proto::Request_Type>& commands)
{
    return paper_client_->check_policies(rcv_clnt_sys_id, request, commands);
}

std::string smpp_gateway::component_id() const
{
    return pinex_->client_id();
}

void smpp_gateway::send_deliver(std::shared_ptr<deliver_info> deliver_info)
{
    smpp_server_->send_deliver(deliver_info);
}

void smpp_gateway::send_delivery_report(const std::string& orig_cp_id, std::shared_ptr<deliver_info> deliver_info)
{
    smpp_server_->send_delivery_report(orig_cp_id, deliver_info);
}
