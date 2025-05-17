#pragma once

#include <pinex/p_client.hpp>

#include <fmt/core.h>
#include <map>

namespace pa::pinex
{
class p_client_list
{
    std::map<std::string, std::shared_ptr<pa::pinex::p_client>> binded_clients_;

    //this use to prevent shared_ptr from deleting
    std::set<std::shared_ptr<pa::pinex::p_client>> p_client_list_;

    using packet_handler_t = std::function<void(const std::string&, uint32_t, const std::string&)>;
    using session_handler_t = std::function<void(const std::string&, session_stat)>;

    int last_index_;

  public:
    explicit p_client_list(boost::asio::io_context* io_context,
                           const std::string& id,
                           const std::vector<std::string>& uri_addresses,
                           uint64_t timeout_sec,
                           bool auto_reconnect,
                           packet_handler_t request_handler,
                           packet_handler_t response_handler,
                           packet_handler_t timeout_handler,
                           session_handler_t session_handler = nullptr)
      : last_index_{0}
    {
        for (auto&& itr : uri_addresses)
        {
            auto clnt = std::make_shared<pa::pinex::p_client>(
                io_context,
                id,
                itr,
                timeout_sec,
                auto_reconnect,
                request_handler,
                response_handler,
                timeout_handler,
                std::bind_front(&p_client_list::on_bind, this),
                std::bind_front(&p_client_list::on_session_close, this),
                session_handler);

            p_client_list_.insert(clnt);
        }
    }

    void stop()
    {
        for (auto& itr : binded_clients_)
        {
            itr.second->stop();
        }
    }

  private:
    void on_bind(const std::string& id, std::shared_ptr<p_client> clnt)
    {
        binded_clients_.insert(std::make_pair(id, clnt));
    }

    void on_error(const std::string& error)
    {
        fmt::print("bind failed, error:{}\n", error);
    }

    void on_session_close(const std::string& id)
    {
        if (!binded_clients_.erase(id))
        {
            fmt::print("could not find client {}\n", id);
        }
    }

  public:
    //TODO: change return value type
    uint32_t send_response(const std::string& msg, uint32_t seq_no, const std::string& id)
    {
        try
        {
            auto clnt = binded_clients_.at(id);
            clnt->send_response(msg, seq_no);

            return seq_no;
        }
        catch (const std::out_of_range& e)
        {
            fmt::print("could not find connection with id {}\n", id);
            return 1;
        }

        return 0;
    }

    uint32_t send_request(const std::string& msg, const std::string& id)
    {
        try
        {
            auto clnt = binded_clients_.at(id);
            auto seq_no = clnt->send_request(msg);

            return seq_no;
        }
        catch (const std::exception& e)
        {
            fmt::print("could not find connection with id {}\n", id);
        }

        return 0;
    }

    std::tuple<uint32_t, std::string> send_request(const std::string& msg)
    {
        if (binded_clients_.size() > 0)
        {
            auto it = binded_clients_.begin();
            std::advance(it, last_index_);
            auto clnt = it->second;
            auto seq_no = clnt->send_request(msg);

            last_index_ = (last_index_ + 1) % binded_clients_.size();

            return {seq_no, it->first};
        }

        return {0, ""};
    }

    uint32_t broad_cast(const std::string& msg)
    {
        for (auto&& itr : binded_clients_)
        {
            itr.second->send_info(msg);
        }

        return 1;
    }
};
}  // namespace pa::pinex