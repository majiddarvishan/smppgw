#pragma once

#include <pa/smpp/net/session.hpp>

#include <set>

namespace pa::smpp
{
class server : public std::enable_shared_from_this<server>
{
  private:
    using authenticate_handler_t = std::function<command_status(const bind_request&, const std::string&)>;
    using bind_handler_t = std::function<void(const bind_request&, std::shared_ptr<session>)>;
    using session_it_t = std::set<std::shared_ptr<session>>::iterator;

    boost::asio::ip::tcp::acceptor acceptor_;
    std::set<std::shared_ptr<session>> binding_sessions_;
    std::string system_id_;
    uint32_t inactivity_threshold_;
    uint32_t enquirelink_threshold_;
    authenticate_handler_t authenticate_handler_;
    bind_handler_t bind_handler_;

  public:
    server(
        boost::asio::io_context* io_context,
        std::string_view ip_address,
        uint16_t port,
        std::string_view system_id,
        uint32_t inactivity_threshold,
        uint32_t enquirelink_threshold,
        authenticate_handler_t authenticate_handler,
        bind_handler_t bind_handler)
        : acceptor_(*io_context)
        , system_id_(system_id)
        , inactivity_threshold_{inactivity_threshold}
        , enquirelink_threshold_{enquirelink_threshold}
        , authenticate_handler_(std::move(authenticate_handler))
        , bind_handler_(std::move(bind_handler))
    {
        try
        {
            auto endpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address(ip_address), port };
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error{ "Failed to listen on " + std::string{ ip_address } + ":" + std::to_string(port) + " error:" + std::string{ ex.what() } };
        }
    }

    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;
    ~server() = default;

    void start()
    {
        do_accept();
    }

  private:
    void do_accept()
    {
        acceptor_.async_accept([this, wptr = weak_from_this()](std::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (wptr.expired())
                return;

            if (ec)
                throw std::runtime_error{ "async_accept failed, error:" + std::string{ ec.message() } };

            on_accept(std::move(socket));

            do_accept();
        });
    }

    void on_accept(boost::asio::ip::tcp::socket socket)
    {
        auto it = binding_sessions_.emplace(std::make_shared<session>(std::move(socket), inactivity_threshold_, enquirelink_threshold_)).first;
        auto session = *it;

        session->close_handler = std::bind_front(&server::on_binding_session_close, this, it);
        session->request_handler = std::bind_front(&server::on_binding_session_request, this, it);

        session->start();
    }

    void on_binding_session_request(session_it_t it, std::shared_ptr<session>, request req, uint32_t sequence_number)
    {
        const auto& session = *it;

        if (auto* bind_request = std::get_if<smpp::bind_request>(&req))
        {
            const auto bind_resp = smpp::bind_resp{ bind_request->bind_type, system_id_, {} };
            const auto [ip_address, port] = session->remote_endpoint();

            const auto status = authenticate_handler_(*bind_request, ip_address);
            session->send(bind_resp, sequence_number, status);
            session->request_handler = {};
            session->bind();

            if (status == command_status::rok)
            {
                session->close_handler = {};
                bind_handler_(*bind_request, session);
                binding_sessions_.erase(it);
            }
        }
    }

    void on_binding_session_close(session_it_t it, std::shared_ptr<session>, const std::optional<std::string>&)
    {
        binding_sessions_.erase(it);
    }
};
}