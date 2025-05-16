#pragma once

//mshadow: fix warning message
#include "src/pinex/pinex.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <prometheus/exposer.h>

// mshadow: todo: its better use include instead of forward declaration
class sgw_logger;
class paper_client;
class sgw_server;

class smpp_gateway : public std::enable_shared_from_this<smpp_gateway>
{
public:
    smpp_gateway(boost::asio::io_context* io_context, pa::config::manager* config_manager, const std::shared_ptr<pa::config::node>& config);

    //smpp_gateway(smpp_gateway& smpp_gateway) = default;
    //smpp_gateway(smpp_gateway const&) = default;
    //smpp_gateway& operator=(smpp_gateway const&) = default;

    //smpp_gateway(smpp_gateway const&);
    //void operator=(smpp_gateway const&);

    virtual ~smpp_gateway() = default;

    void start();
    void stop();
    void initilize();
    bool is_run() const;

    bool check_policies(const std::string&                                       rcv_clnt_sys_id,
                        std::shared_ptr<submit_info>                             request,
                        const std::set<pa::paper::proto::Request_Type>& commands);

    template<typename info_type>
    bool send_to_boninet(int msg_type, info_type pdu) //todo
    {
        return pinex_->send(msg_type, pdu);
    }

    std::string component_id() const;
    void get_current_time(unsigned& hours, unsigned& minutes, unsigned& seconds);

    void send_deliver(std::shared_ptr<deliver_info> deliver_info);
    void send_delivery_report(const std::string& orig_cp_id, std::shared_ptr<deliver_info> deliver_info);

private:
    void do_set_timer();

    time_t start_time_;

    std::shared_ptr<sgw_server> smpp_server_;
    std::shared_ptr<pinex> pinex_;
    std::shared_ptr<paper_client> paper_client_;
    std::shared_ptr<sgw_logger> logger_;

    boost::asio::io_context* io_context_;
    pa::config::manager* config_manager_;
    std::shared_ptr<pa::config::node> config_;
    boost::asio::steady_timer timer_;
    std::atomic<bool> run_;

    std::unique_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
};
