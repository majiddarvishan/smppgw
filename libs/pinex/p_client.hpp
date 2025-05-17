#pragma once

#include <pinex/pinex.hpp>
#include <pinex/common/helpers.hpp>
#include <pinex/io/expirator.hpp>

#include <any>
#include <map>
#include <fmt/core.h>

namespace pa::pinex
{
class p_client : public std::enable_shared_from_this<p_client>
{
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<pa::pinex::client> tcp_client_;
    std::shared_ptr<pa::pinex::session> binded_session_;
    std::string client_id_;
    std::string server_id_;
    std::chrono::seconds timeout_sec_;

    bool auto_reconnect_;

    using packet_handler_t = std::function<void(const std::string&, uint32_t, const std::string&)>;

    packet_handler_t request_handler_;
    packet_handler_t response_handler_;
    packet_handler_t timeout_handler_;

    using bind_handler_t = std::function<void(const std::string&, std::shared_ptr<p_client>)>;
    using close_handler_t = std::function<void(const std::string&)>;
    using session_handler_t = std::function<void(const std::string&, session_stat)>;

    bind_handler_t bind_handler_;
    close_handler_t close_handler_;
    session_handler_t session_handler_;

    std::shared_ptr<pa::pinex::io::expirator<uint32_t, std::any>> packet_expirator_;

  public:
    explicit p_client(boost::asio::io_context* io_context,
                      const std::string& id,
                      const std::string& uri_address,
                      uint64_t timeout_sec,
                      bool auto_reconnect,
                      packet_handler_t request_handler,
                      packet_handler_t response_handler,
                      packet_handler_t timeout_handler,
                      bind_handler_t bind_handler = nullptr,
                      close_handler_t close_handler = nullptr,
                      session_handler_t session_handler = nullptr)
      : acceptor_(*io_context)
      , client_id_(id)
      , timeout_sec_{std::chrono::seconds(timeout_sec)}
      , auto_reconnect_{auto_reconnect}
      , request_handler_(std::move(request_handler))
      , response_handler_(std::move(response_handler))
      , timeout_handler_(std::move(timeout_handler))
      , bind_handler_(std::move(bind_handler))
      , close_handler_(std::move(close_handler))
      , session_handler_(std::move(session_handler))
    {
        auto uri = split(uri_address, ':');

        tcp_client_ = std::make_shared<pa::pinex::client>(
            io_context,
            uri[0],
            std::stoi(uri[1]),
            pa::pinex::bind_request{.bind_type = pa::pinex::bind_type::bi_direction, .system_id = id},
            std::bind_front(&p_client::on_bind, this),
            std::bind_front(&p_client::on_error, this));

        tcp_client_->start();

        packet_expirator_ = std::make_shared<io::expirator<uint32_t, std::any>>(
            io_context,
            std::chrono::milliseconds{1},
            std::bind_front(&p_client::on_session_timeout, this));

        packet_expirator_->start();
    }

    void start()
    {
        tcp_client_->start();
    }

    void stop()
    {
        tcp_client_.reset();

        if (!binded_session_)
            return;

        fmt::print("unbinding binded session...\n");

        binded_session_->unbind();
    }

  private:
    void on_bind(const pa::pinex::bind_response& bind_resp, std::shared_ptr<pa::pinex::session> session)
    {
        fmt::print("client: {} has been successfully binded, server system_id:{}\n", client_id_, bind_resp.system_id);

        session->request_handler = std::bind_front(&p_client::on_session_request, this, bind_resp.system_id);
        session->response_handler = std::bind_front(&p_client::on_session_response, this, bind_resp.system_id);
        session->close_handler = std::bind_front(&p_client::on_session_close, this, bind_resp.system_id);
        session->deserialization_error_handler = std::bind_front(&p_client::on_session_deserialization_error, this);
        session->send_buf_available_handler = std::bind_front(&p_client::on_session_send_buf_available, this, bind_resp.system_id);

        server_id_ = bind_resp.system_id;

        if (session_handler_)
            session_handler_(server_id_, session_stat::bind);

        binded_session_ = session;

        if (bind_handler_)
        {
            bind_handler_(bind_resp.system_id, shared_from_this());
        }
    }

    void on_error(const std::string& error)
    {
        fmt::print("bind failed, error:{}\n", error);
    }

    void on_session_close(const std::string& server_id, std::optional<std::string> error)
    {
        if (error)
        {
            fmt::print("session has been closed on error:{}\n", error.value());
        }
        else
        {
            fmt::print("session has been closed gracefully\n");
        }

        binded_session_.reset();

        if (close_handler_)
        {
            close_handler_(server_id);
        }

        if (session_handler_)
            session_handler_(server_id_, session_stat::close);

        if (auto_reconnect_)
            start();
    }

    void on_session_deserialization_error(const std::string& error, pa::pinex::command_id command_id, std::span<const uint8_t> body)
    {
        fmt::print("error in pdu deserialization, error: {} command_id: {} body-size: {}\n", error, static_cast<uint32_t>(command_id), body.size());
    }

    void on_session_send_buf_available([[maybe_unused]] const std::string& server_id)
    {
        fmt::print("send buf become available again\n");
        binded_session_->resume_receiving();
    }

    void on_session_request(const std::string& server_id, pa::pinex::request&& req_packet, uint32_t sequence_number)
    {
        std::visit(
            [this, server_id, sequence_number](auto&& req) {
                using request_type = std::decay_t<decltype(req)>;
                if constexpr (std::is_same_v<request_type, pa::pinex::stream_request>)
                {
                    pa::pinex::stream_request sm = std::move(req);
                    request_handler_(server_id, sequence_number, sm.message_body);
                }
                else
                {
                    fmt::print("invalid packet type\n");
                }
            },
            req_packet);
    }

    void on_session_response(const std::string& server_id,
                             pa::pinex::response&& resp_packet,
                             uint32_t sequence_number,
                             [[maybe_unused]] pa::pinex::command_status command_status)
    {
        std::visit(
            [this, server_id, sequence_number](auto&& resp) {
                using response_type = std::decay_t<decltype(resp)>;
                if constexpr (std::is_same_v<response_type, pa::pinex::stream_response>)
                {
                    pa::pinex::stream_response sm = std::move(resp);

                    packet_expirator_->remove(sequence_number);

                    response_handler_(server_id, sequence_number, sm.message_body);
                }
                else
                {
                    fmt::print("invalid packet type\n");
                }
            },
            resp_packet);
    }

    void on_session_timeout(uint32_t sequence_number, std::any user_data)
    {
        auto sm = std::any_cast<pa::pinex::stream_request>(user_data);
        timeout_handler_(server_id_, sequence_number, sm.message_body);
    }

  public:
    uint32_t send_response(const std::string& msg, uint32_t seq_no)
    {
        binded_session_->send_response(pa::pinex::stream_response{.message_body = msg}, seq_no, pa::pinex::command_status::rok);

        // if (binded_session_->is_send_buf_above_threshold())
        // {
        //     fmt::print("send buffer is above the threshold\n");
        //     binded_session_->pause_receiving();
        // }

        return seq_no;
    }

    uint32_t send_request(const std::string& msg)
    {
        pa::pinex::stream_request pdu{.message_body = msg};

        auto seq_no = binded_session_->send_request(pdu);

        packet_expirator_->add(seq_no, timeout_sec_, pdu);

        // if (binded_session_->is_send_buf_above_threshold())
        // {
        //     fmt::print("send buffer is above the threshold\n");
        //     binded_session_->pause_receiving();
        // }

        return seq_no;
    }

    uint32_t send_info(const std::string& msg)
    {
        pa::pinex::stream_request pdu{.message_body = msg};

        auto seq_no = binded_session_->send_request(pdu);

        // if (binded_session_->is_send_buf_above_threshold())
        // {
        //     fmt::print("send buffer is above the threshold\n");
        //     binded_session_->pause_receiving();
        // }

        return seq_no;
    }
};
}  // namespace pa::pinex