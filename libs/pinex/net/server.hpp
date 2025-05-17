#pragma once

#include <pinex/net/session.hpp>

#include <set>

namespace pa::pinex
{
class server : public std::enable_shared_from_this<server>
{
  private:
    using bind_handler_t = std::function<bool(const bind_request&, std::shared_ptr<session>)>;
    using session_it_t = std::set<std::shared_ptr<session>>::iterator;

    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::io_context* io_context_;
    std::set<std::shared_ptr<session>> binding_sessions_;
    std::string system_id_;
    bind_handler_t bind_handler_;

  public:
    server(
        boost::asio::io_context* io_context,
        std::string_view ip_address,
        uint16_t port,
        std::string_view system_id,
        bind_handler_t bind_handler)
      : acceptor_(*io_context)
      , io_context_{io_context}
      , system_id_(system_id)
      , bind_handler_(std::move(bind_handler))
    {
        try
        {
            auto endpoint = boost::asio::ip::tcp::endpoint{boost::asio::ip::make_address(ip_address), port};
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error{"Failed to listen on " + std::string{ip_address} + ":" + std::to_string(port) + " error:" + std::string{ex.what()}};
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
                throw std::runtime_error{"async_accept failed, error:" + std::string{ec.message()}};

            on_accept(std::move(socket));

            do_accept();
        });
    }

    void on_accept(boost::asio::ip::tcp::socket socket)
    {
        auto it = binding_sessions_.emplace(std::make_shared<session>(io_context_, std::move(socket))).first;
        auto session = *it;

        session->close_handler = std::bind_front(&server::on_binding_session_close, this, it);
        session->request_handler = std::bind_front(&server::on_binding_session_request, this, it);

        session->start();
    }

    void on_binding_session_request(session_it_t it, request req, uint32_t sequence_number)
    {
        auto session = *it;

        if (auto* bind_request = std::get_if<pinex::bind_request>(&req))
        {
            const auto bind_resp = pinex::bind_response{bind_request->bind_type, system_id_};
            const auto [ip_address, port] = session->remote_endpoint();

            session->request_handler = {};
            session->close_handler = {};

            if(bind_handler_(*bind_request, session))
                session->send_response(bind_resp, sequence_number, command_status::rok);
            else
                session->send_response(bind_resp, sequence_number, command_status::rfail);

            binding_sessions_.erase(it);
        }
    }

    void on_binding_session_close(session_it_t it, const std::optional<std::string>&)
    {
        binding_sessions_.erase(it);
    }
};
}  // namespace pa::pinex