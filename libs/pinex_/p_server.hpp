#pragma once

#include <pa/pinex.hpp>
#include <pa/pinex/common/helpers.hpp>
#include <pa/pinex/io/expirator.hpp>

#include <fmt/core.h>
#include <map>

namespace pa::pinex
{
class p_server
{
    boost::asio::io_context* acceptor_;
    std::shared_ptr<pa::pinex::server> tcp_server_;
    std::map<std::string, std::shared_ptr<pa::pinex::session>> binded_sessions_;

    using packet_handler_t = std::function<void(const std::string&, uint32_t, const std::string&)>;
    using session_handler_t = std::function<void(const std::string&, session_stat)>;

    std::chrono::seconds timeout_sec_;

    packet_handler_t request_handler_;
    packet_handler_t response_handler_;
    packet_handler_t timeout_handler_;
    session_handler_t session_handler_;

    std::map<std::string, std::shared_ptr<pa::pinex::io::expirator<uint32_t, std::any>>> packet_expirator_;

    int last_index_ = 0;

  public:
    explicit p_server(
        boost::asio::io_context* io_context,
        std::string_view system_id,
        const std::string& uri_address,
        uint64_t timeout_sec,
        packet_handler_t request_handler,
        packet_handler_t response_handler,
        packet_handler_t timeout_handler,
        session_handler_t session_handler = nullptr)
      : acceptor_(io_context)
      , timeout_sec_{std::chrono::seconds(timeout_sec)}
      , request_handler_(std::move(request_handler))
      , response_handler_(std::move(response_handler))
      , timeout_handler_(std::move(timeout_handler))
      , session_handler_(std::move(session_handler))
    {
        auto uri = split(uri_address, ':');

        tcp_server_ = std::make_shared<pa::pinex::server>(io_context,
                                                         uri[0],
                                                         std::stoi(uri[1]),
                                                         system_id,
                                                         std::bind_front(&p_server::on_bind, this));

        tcp_server_->start();
    }

    void stop()
    {
        tcp_server_.reset();

        if (binded_sessions_.empty())
            return;

        fmt::print("unbinding binded sessions...\n");

        for (auto& session : binded_sessions_)
            session.second->unbind();
    }

  private:
    bool on_bind(const pa::pinex::bind_request& bind_request, std::shared_ptr<pa::pinex::session> session)
    {
        fmt::print("new session has been binded system_id:{}\n", bind_request.system_id);

        if(binded_sessions_.contains(bind_request.system_id))
        {
            fmt::print("session with id {} is already exist\n", bind_request.system_id);
            return false;
        }
        
        binded_sessions_.insert(std::make_pair(bind_request.system_id, session));

        {
            auto packet_expirator = std::make_shared<io::expirator<uint32_t, std::any>>(
                acceptor_,
                std::chrono::milliseconds{1},
                std::bind_front(&p_server::on_session_timeout, this, bind_request.system_id));

            packet_expirator->start();

            packet_expirator_.insert(std::make_pair(bind_request.system_id, packet_expirator));
        }

        if (session_handler_)
            session_handler_(bind_request.system_id, session_stat::bind);

        session->request_handler = std::bind_front(&p_server::on_session_request, this, bind_request.system_id);
        session->response_handler = std::bind_front(&p_server::on_session_response, this, bind_request.system_id);
        session->close_handler = std::bind_front(&p_server::on_session_close, this, bind_request.system_id);
        session->deserialization_error_handler = std::bind_front(&p_server::on_session_deserialization_error, this);
        session->send_buf_available_handler = std::bind_front(&p_server::on_session_send_buf_available, this, bind_request.system_id);

        return true;
    }

    void on_session_close(const std::string& client_id, std::optional<std::string> error)
    {
        if(!binded_sessions_.erase(client_id))
        {
            fmt::print("could not find client {}\n", client_id);
            return;
        }

        if (error)
            fmt::print("session has been closed on error:{}\n", error.value());
        else
            fmt::print("session has been closed gracefully\n");

        packet_expirator_.erase(client_id);

        if (session_handler_)
            session_handler_(client_id, session_stat::close);
    }

    void on_session_request(const std::string& client_id, pa::pinex::request&& req, uint32_t sequence_number)
    {
        std::visit(
            [this, client_id, sequence_number](auto&& r) {
                using request_type = std::decay_t<decltype(r)>;
                if constexpr (std::is_same_v<request_type, pa::pinex::stream_request>)
                {
                    pa::pinex::stream_request sm = std::move(r);
                    request_handler_(client_id, sequence_number, sm.message_body);
                }
                else
                {
                    fmt::print("invalid packet type\n");
                }
            },
            req);
    }

    void on_session_response(const std::string& client_id, pa::pinex::response&& resp, uint32_t sequence_number, [[maybe_unused]] pa::pinex::command_status status)
    {
        std::visit(
            [this, client_id, sequence_number](auto&& r) {
                using response_type = std::decay_t<decltype(r)>;
                if constexpr (std::is_same_v<response_type, pa::pinex::stream_response>)
                {
                    pa::pinex::stream_response sm = std::move(r);

                    packet_expirator_.at(client_id)->remove(sequence_number);

                    response_handler_(client_id, sequence_number, sm.message_body);
                }
                else
                {
                    fmt::print("invalid packet type\n");
                }
            },
            resp);
    }

    void on_session_timeout(const std::string& client_id, uint32_t seq_no, std::any user_data)
    {
        auto sm = std::any_cast<pa::pinex::stream_request>(user_data);
        timeout_handler_(client_id, seq_no, sm.message_body);
    }

    void on_session_deserialization_error(const std::string& error, pa::pinex::command_id command_id, std::span<const uint8_t> body)
    {
        fmt::print("error in pdu deserialization, error: {} command_id: {} body-size: {}\n", error, static_cast<uint32_t>(command_id), body.size());
    }

    void on_session_send_buf_available(const std::string& client_id)
    {
        fmt::print("send buf become available again\n");

        try
        {
            binded_sessions_.at(client_id)->resume_receiving();
        }
        catch (const std::out_of_range& e)
        {
            fmt::print("could not find client {}\n", client_id);
        }
    }

  public:
    uint32_t send_response(const std::string& msg, uint32_t seq_no, const std::string& client_id)
    {
        try
        {
            auto session = binded_sessions_.at(client_id);
            session->send_response(pa::pinex::stream_response{.message_body = msg}, seq_no, pa::pinex::command_status::rok);

            // if (session->is_send_buf_above_threshold())
            // {
            //     fmt::print("send buffer is above the threshold\n");
            //     session->pause_receiving();
            // }

            return seq_no;
        }
        catch (const std::out_of_range& e)
        {
            fmt::print("could not find client {}\n", client_id);
        }

        return 0;
    }

    uint32_t send_request(const std::string& msg, const std::string& client_id)
    {
        pa::pinex::stream_request pdu{.message_body = msg};

        try
        {
            auto session = binded_sessions_.at(client_id);
            auto seq_no = session->send_request(pdu);

            packet_expirator_.at(client_id)->add(seq_no, timeout_sec_, pdu);

            // if (session->is_send_buf_above_threshold())
            // {
            //     fmt::print("send buffer is above the threshold\n");
            //     session->pause_receiving();
            // }

            return seq_no;
        }
        catch (const std::exception& e)
        {
            fmt::print("could not find client {}\n", client_id);
        }

        return 0;
    }

    std::tuple<uint32_t, std::string> send_request(const std::string& msg)
    {
        if (binded_sessions_.size() > 0)
        {
            pa::pinex::stream_request pdu{.message_body = msg};

            auto it = binded_sessions_.begin();
            std::advance(it, last_index_);
            auto session = it->second;
            auto seq_no = session->send_request(pdu);

            packet_expirator_.at(it->first)->add(seq_no, timeout_sec_, pdu);

            last_index_ = (last_index_ + 1) % binded_sessions_.size();

            // if (session->is_send_buf_above_threshold())
            // {
            //     fmt::print("send buffer is above the threshold\n");
            //     session->pause_receiving();
            // }

            return {seq_no, it->first};
        }

        return {0, ""};
    }

    uint32_t broad_cast(const std::string& msg)
    {
        pa::pinex::stream_request pdu{.message_body = msg};

        for (auto&& itr : binded_sessions_)
        {
            itr.second->send_request(pdu);
        }

        return 1;
    }
};
}  // namespace pa::pinex