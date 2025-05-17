#pragma once

#include <pinex/net/session.hpp>

namespace pa::pinex
{
class client : public std::enable_shared_from_this<client>
{
  private:
    using bind_handler_t = std::function<void(const bind_response&, std::shared_ptr<session>)>;
    using error_handler_t = std::function<void(const std::string&)>;

    boost::asio::ip::tcp::endpoint endpoint_;
    boost::asio::io_context* io_context_;
    bind_request bind_request_;
    boost::asio::steady_timer reconnect_timer_;
    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<session> binding_session_;
    bind_handler_t bind_handler_;
    error_handler_t error_handler_;

  public:
    client(boost::asio::io_context* io_context, std::string_view ip_address, uint16_t port, bind_request bind_request, bind_handler_t bind_handler, error_handler_t error_handler)
      : endpoint_(boost::asio::ip::make_address(ip_address), port)
      , io_context_{io_context}
      , bind_request_(std::move(bind_request))
      , reconnect_timer_(*io_context)
      , socket_(*io_context)
      , bind_handler_(std::move(bind_handler))
      , error_handler_(std::move(error_handler))
    {
    }

    client(const client&) = delete;
    client& operator=(const client&) = delete;
    client(client&&) = delete;
    client& operator=(client&&) = delete;
    ~client() = default;

    void start()
    {
        do_connect();
    }

  private:
    void do_set_retry_timer()
    {
        reconnect_timer_.expires_after(std::chrono::seconds{5});
        reconnect_timer_.async_wait([this, wptr = weak_from_this()](std::error_code ec) {
            if (wptr.expired())
                return;

            if (ec)
                return on_error("Retry timer failed, error:" + std::string{ec.message()});

            do_connect();
        });
    }

    void do_connect()
    {
        socket_.async_connect(endpoint_, [this, wptr = weak_from_this()](boost::system::error_code ec) {
            if (wptr.expired())
                return;

            if (ec)
            {
                socket_.close(ec);
                do_set_retry_timer();
                return;
            }

            on_connect();
        });
    }

    void on_connect()
    {
        binding_session_ = std::make_shared<session>(io_context_, std::move(socket_));

        binding_session_->close_handler = std::bind_front(&client::on_binding_session_close, this);
        binding_session_->response_handler = std::bind_front(&client::on_binding_session_response, this);

        binding_session_->start();

        binding_session_->send_request(bind_request_);
    }

    void on_binding_session_response(response resp, uint32_t /*sequence_number*/, command_status command_status)
    {
        if (auto* bind_resp = std::get_if<pinex::bind_response>(&resp))
        {
            if (command_status == command_status::rok)
            {
                binding_session_->response_handler = {};
                binding_session_->close_handler = {};
                binding_session_->pause_receiving();

                // defer handler for execution, so it would be safe to destroy this object inside the handler
                boost::asio::defer(socket_.get_executor(), [wptr = weak_from_this(), bind_resp = *bind_resp, session = this->binding_session_, handler = this->bind_handler_] {
                    if (wptr.expired())
                        return;

                    handler(bind_resp, session);
                    session->resume_receiving();
                });

                binding_session_.reset();
            }
            else
            {
                on_error("Bind request rejected by the server");
                binding_session_.reset();
            }
        }
    }

    void on_binding_session_close(const std::optional<std::string>& error)
    {
        on_error("Session has been closed during binding, error:" + error.value_or("none"));
        binding_session_.reset();
    }

    void on_error(const std::string& error)
    {
        // defer handler for execution, so it would be safe to destroy this object inside the handler
        boost::asio::post(socket_.get_executor(), [wptr = weak_from_this(), error, handler = this->error_handler_] {
            if (wptr.expired())
                return;

            handler(error);
        });
    }
};
}  // namespace pa::pinex