#pragma once

#include <smpp/common.hpp>
#include <smpp/net/detail/flat_buffer.hpp>
#include <smpp/pdu.hpp>

#include <boost/asio.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace pa::smpp
{
using request = std::variant<std::monostate, bind_request, query_sm, submit_sm, deliver_sm, replace_sm, cancel_sm, alert_notification, data_sm>;
using response = std::variant<std::monostate, generic_nack, bind_resp, query_sm_resp, submit_sm_resp, deliver_sm_resp, replace_sm_resp, cancel_sm_resp, data_sm_resp>;

class session : public std::enable_shared_from_this<session>
{
  public:
    std::function<void(std::shared_ptr<session>, std::optional<std::string>)> close_handler;                                               /* required */
    std::function<void(std::shared_ptr<session>, request&&, uint32_t)> request_handler;                                                    /* optional */
    std::function<void(std::shared_ptr<session>, response&&, uint32_t, command_status)> response_handler;                                  /* optional */
    std::function<void(std::shared_ptr<session>)> send_buf_available_handler;                                                              /* optional */
    std::function<void(std::shared_ptr<session>, const std::string&, command_id, std::span<const uint8_t>)> deserialization_error_handler; /* optional */

  private:
    enum class state
    {
        open,
        unbinding,
        close,
    };

    enum class receiving_state
    {
        receiving,
        pending_pause,
        paused
    };

    static constexpr size_t header_length{ 16 };

    state state_{ state::open };
    receiving_state receiving_state_{ receiving_state::paused };
    uint32_t sequence_number_{ 0 };

    boost::asio::ip::tcp::socket socket_;

    uint32_t inactivity_counter_{0};
    uint32_t enquirelink_counter_{0};

    uint32_t inactivity_threshold_;
    uint32_t enquirelink_threshold_;

    boost::asio::steady_timer inactivity_timer_;
    boost::asio::steady_timer enquirelink_timer_;

    std::vector<uint8_t> writing_send_buf_;
    std::vector<uint8_t> pending_send_buf_;
    size_t send_buf_threshold_{ 1024 * 1024 };
    detail::flat_buffer<uint8_t, 1024 * 1024> receive_buf_{};

  public:
    explicit session(boost::asio::ip::tcp::socket socket,
                     uint32_t inactivity_threshold,
                     uint32_t enquirelink_threshold)
        : socket_(std::move(socket))
        , inactivity_threshold_{inactivity_threshold}
        , enquirelink_threshold_{enquirelink_threshold}
        , inactivity_timer_(socket_.get_executor())
        , enquirelink_timer_(socket_.get_executor())
    {
    }

    session(const session&) = delete;
    session& operator=(const session&) = delete;
    session(session&&) = delete;
    session& operator=(session&&) = delete;
    ~session() = default;

    void start()
    {
        do_set_inactivity_timer();

        resume_receiving();
    }

    std::tuple<std::string, uint16_t> remote_endpoint() const
    {
        auto endpoint = socket_.remote_endpoint();
        return { endpoint.address().to_string(), endpoint.port() };
    }

    bool is_open() const
    {
        return state_ == state::open;
    }

    void unbind(bool force = false)
    {
        if (state_ == state::open)
        {
            state_ = state::unbinding;

            do_unset_enquirelink_timer();

            if(!force)
                send_command(command_id::unbind);
        }
    }

    void bind()
    {
        if (state_ == state::open)
        {
            do_set_enquirelink_timer();
        }
    }

    template<typename PDU>
    void send(const PDU& pdu, uint32_t sequence_number, command_status command_status)
    {
        static_assert(detail::is_response<PDU>, "PDU isn't a response");

        send_impl(pdu, sequence_number, command_status);
    }

    template<typename PDU>
    uint32_t send(const PDU& pdu)
    {
        static_assert(!detail::is_response<PDU>, "PDU isn't a request");

        auto sequence_number = next_sequence_number();

        send_impl(pdu, sequence_number, command_status::rok);

        return sequence_number;
    }

    void set_send_buf_threshold(size_t size)
    {
        send_buf_threshold_ = size;
    }

    bool is_send_buf_above_threshold() const
    {
        return pending_send_buf_.size() > send_buf_threshold_;
    }

    void pause_receiving()
    {
        if (receiving_state_ == receiving_state::receiving)
            receiving_state_ = receiving_state::pending_pause;
    }

    void resume_receiving()
    {
        auto prev_receiving_state = std::exchange(receiving_state_, receiving_state::receiving);
        if (prev_receiving_state == receiving_state::paused)
            do_receive();
    }

  private:
    void close(const std::string& reason)
    {
        if (state_ == state::close)
            return;

        std::optional<std::string> opt_error;
        if (state_ == state::open)
            opt_error = reason;

        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);

        state_ = state::close;

        /* defer handler for execution, so it would be safe to destroy this object inside the handler */
        boost::asio::post(socket_.get_executor(), [wptr = weak_from_this(), opt_error, handler = this->close_handler] {
            if (wptr.expired())
                return;

            handler(wptr.lock(), opt_error);

            auto sptr = wptr.lock();
            if(sptr)
            {
                sptr->request_handler = {};
                sptr->response_handler = {};
                sptr->send_buf_available_handler = {};
                sptr->close_handler = {};
                sptr->deserialization_error_handler = {};
                sptr->do_unset_enquirelink_timer();
            }
        });
    }

    uint32_t next_sequence_number()
    {
        if (++sequence_number_ > 0x7FFFFFFF)
            sequence_number_ = 1;
        return sequence_number_;
    }

    void do_set_inactivity_timer()
    {
        inactivity_timer_.expires_after(std::chrono::seconds{ inactivity_threshold_ });
        inactivity_timer_.async_wait([this, wptr = weak_from_this()](std::error_code ec) {
            if (wptr.expired())
                return;

            if (ec)
                return close(ec.message());

            if (inactivity_counter_ >= 1)
                return close("Inactivity timer is reached");

            inactivity_counter_++;

            do_set_inactivity_timer();
        });
    }

    void do_set_enquirelink_timer()
    {
        enquirelink_timer_.expires_after(std::chrono::seconds{ enquirelink_threshold_ });
        enquirelink_timer_.async_wait([this, wptr = weak_from_this()](std::error_code ec) {
            if (wptr.expired())
                return;

            if (ec)
                return close(ec.message());

            if (enquirelink_counter_ >= 1 && state_ == state::open)
                send_command(command_id::enquire_link);

            enquirelink_counter_++;

            do_set_enquirelink_timer();
        });
    }

    void do_unset_enquirelink_timer()
    {
        enquirelink_timer_.cancel();
    }

    void do_receive()
    {
        while (true)
        {
            if (receiving_state_ != receiving_state::receiving)
                break;

            if (receive_buf_.size() < header_length)
                break;

            auto header_buf = std::span{ receive_buf_ }.subspan<0, header_length>();

            auto [command_length, command_id, command_status, sequence_number] = deserialize_header(header_buf);

            if (receive_buf_.size() < command_length)
                break;

            auto body_buf = std::span{ receive_buf_.begin() + header_length, receive_buf_.begin() + command_length };

            if (is_response(command_id))
            {
                consume_response_pdu(command_id, command_status, sequence_number, body_buf);
            }
            else
            {
                consume_request_pdu(command_id, command_status, sequence_number, body_buf);
            }

            receive_buf_.consume(command_length);
        }

        if (receiving_state_ == receiving_state::pending_pause)
        {
            receiving_state_ = receiving_state::paused;
            return;
        }

        socket_.async_receive(receive_buf_.prepare(64 * 1024), [this, wptr = weak_from_this()](std::error_code ec, size_t received) {
            if (wptr.expired())
                return;

            if (ec)
                return close(ec.message());

            receive_buf_.commit(received);

            inactivity_counter_ = 0;
            if (state_ == state::open)
                enquirelink_counter_ = 0;

            do_receive();
        });
    }

    void consume_response_pdu(command_id command_id, command_status command_status, uint32_t sequence_number, std::span<const uint8_t> buf)
    {
        response resp;

        try
        {
            switch (command_id)
            {
                case command_id::generic_nack:
                    resp = deserialize<generic_nack>(buf);
                    break;
                case command_id::enquire_link_resp:
                    break;
                case command_id::unbind_resp:
                    close("unbind_resp received");
                    break;
                case command_id::bind_transmitter_resp:
                    resp = deserialize<bind_resp>(buf, bind_type::transmitter);
                    break;
                case command_id::bind_receiver_resp:
                    resp = deserialize<bind_resp>(buf, bind_type::receiver);
                    break;
                case command_id::bind_transceiver_resp:
                    resp = deserialize<bind_resp>(buf, bind_type::transceiver);
                    break;
                case command_id::query_sm_resp:
                    resp = deserialize<query_sm_resp>(buf);
                    break;
                case command_id::submit_sm_resp:
                    resp = deserialize<submit_sm_resp>(buf);
                    break;
                case command_id::deliver_sm_resp:
                    resp = deserialize<deliver_sm_resp>(buf);
                    break;
                case command_id::replace_sm_resp:
                    resp = deserialize<replace_sm_resp>(buf);
                    break;
                case command_id::cancel_sm_resp:
                    resp = deserialize<cancel_sm_resp>(buf);
                    break;
                case command_id::data_sm_resp:
                    resp = deserialize<data_sm_resp>(buf);
                    break;
                default:
                    throw std::logic_error{ "Unknown pdu" };
            }
        }
        catch (const std::exception& ex)
        {
            if (deserialization_error_handler)
                deserialization_error_handler(shared_from_this(), std::string{ ex.what() }, command_id, buf);
        }

        if (resp.index() != 0 && state_ == state::open && response_handler)
            response_handler(shared_from_this(), std::move(resp), sequence_number, command_status);
    }

    void consume_request_pdu(command_id command_id, command_status /* command_status */, uint32_t sequence_number, std::span<const uint8_t> buf)
    {
        request req;

        try
        {
            switch (command_id)
            {
                case command_id::enquire_link:
                    send_command(command_id::enquire_link_resp, sequence_number);
                    break;
                case command_id::unbind:
                    if (state_ == state::open)
                        state_ = state::unbinding;
                    send_command(command_id::unbind_resp, sequence_number);
                    break;
                case command_id::bind_transmitter:
                    req = deserialize<bind_request>(buf, bind_type::transmitter);
                    break;
                case command_id::bind_receiver:
                    req = deserialize<bind_request>(buf, bind_type::receiver);
                    break;
                case command_id::bind_transceiver:
                    req = deserialize<bind_request>(buf, bind_type::transceiver);
                    break;
                case command_id::query_sm:
                    req = deserialize<query_sm>(buf);
                    break;
                case command_id::submit_sm:
                    req = deserialize<submit_sm>(buf);
                    break;
                case command_id::deliver_sm:
                    req = deserialize<deliver_sm>(buf);
                    break;
                case command_id::replace_sm:
                    req = deserialize<replace_sm>(buf);
                    break;
                case command_id::cancel_sm:
                    req = deserialize<cancel_sm>(buf);
                    break;
                case command_id::alert_notification:
                    req = deserialize<alert_notification>(buf);
                    break;
                case command_id::data_sm:
                    req = deserialize<data_sm>(buf);
                    break;
                default:
                    send_command(command_id::generic_nack, sequence_number, command_status::rinvcmdid);
                    throw std::logic_error{ "Unknown pdu" };
            }
        }
        catch (const std::exception& ex)
        {
            if (deserialization_error_handler)
                deserialization_error_handler(shared_from_this(), std::string{ ex.what() }, command_id, buf);
        }

        if (req.index() != 0 && state_ == state::open && request_handler)
            request_handler(shared_from_this(), std::move(req), sequence_number);
    }

    template<typename PDU>
    void send_impl(const PDU& pdu, uint32_t sequence_number, command_status cmd_status)
    {
        if (state_ == state::close)
            throw std::logic_error{ "Send on closed session" };

        if (state_ == state::unbinding)
            throw std::logic_error{ "Send on unbinding session" };

        auto prev_size = pending_send_buf_.size();

        /* reserver for header (because we don't know command length before serializing the PDU) */
        pending_send_buf_.resize(prev_size + header_length);

        try
        {
            serialize_to(&pending_send_buf_, pdu);
        }
        catch (...)
        {
            pending_send_buf_.resize(prev_size); /* remove appended data due to incomplete serialization */
            throw;
        }

        auto command_length = pending_send_buf_.size() - prev_size;

        auto header = serialize_header(command_length, detail::command_id_of(pdu), sequence_number, cmd_status);

        std::copy(header.begin(), header.end(), pending_send_buf_.begin() + static_cast<std::ptrdiff_t>(prev_size));

        do_send();
    }

    uint32_t send_command(command_id command_id)
    {
        auto sequence_number = next_sequence_number();

        send_command(command_id, sequence_number);

        return sequence_number;
    }

    void send_command(command_id command_id, uint32_t sequence_number, command_status cmd_status = command_status::rok)
    {
        auto header = serialize_header(header_length, command_id, sequence_number, cmd_status);

        pending_send_buf_.insert(pending_send_buf_.end(), header.begin(), header.end());

        do_send();
    }

    void do_send()
    {
        if (!writing_send_buf_.empty())
            return; /* ongoing async_write would call this function after it finished */

        std::swap(writing_send_buf_, pending_send_buf_);

        /* if pending_send_buf_ has been above the threshold, notify it becomes available again */
        if (writing_send_buf_.size() > send_buf_threshold_ && send_buf_available_handler)
            send_buf_available_handler(shared_from_this());

        boost::asio::async_write(socket_, boost::asio::buffer(writing_send_buf_), [this, wptr = weak_from_this()](std::error_code ec, size_t) {
            if (wptr.expired())
                return;

            if (ec)
                return close(ec.message());

            writing_send_buf_.clear();

            if (!pending_send_buf_.empty())
                do_send();
        });
    }

    static bool is_response(command_id command_id)
    {
        return static_cast<uint32_t>(command_id) & 0x80000000;
    }

    static std::tuple<uint32_t, command_id, command_status, uint32_t> deserialize_header(std::span<const uint8_t, header_length> buf)
    {
        auto deserialize_u32 = [](std::span<const uint8_t, 4> data) -> uint32_t { return data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]; };

        auto command_length = deserialize_u32(buf.subspan<0, 4>());
        auto command_id = static_cast<smpp::command_id>(deserialize_u32(buf.subspan<4, 4>()));
        auto command_status = static_cast<smpp::command_status>(deserialize_u32(buf.subspan<8, 4>()));
        auto sequence_number = deserialize_u32(buf.subspan<12, 4>());

        return { command_length, command_id, command_status, sequence_number };
    }

    static std::array<uint8_t, header_length> serialize_header(uint32_t command_length, command_id command_id, uint32_t sequence_number, command_status command_status)
    {
        std::array<uint8_t, header_length> buf{};

        auto serialize_u32 = [](std::span<uint8_t, 4> data, uint32_t val) {
            data[0] = (val >> 24) & 0xFF;
            data[1] = (val >> 16) & 0xFF;
            data[2] = (val >> 8) & 0xFF;
            data[3] = (val >> 0) & 0xFF;
        };

        serialize_u32(std::span{ buf }.subspan<0, 4>(), command_length);
        serialize_u32(std::span{ buf }.subspan<4, 4>(), static_cast<uint32_t>(command_id));
        serialize_u32(std::span{ buf }.subspan<8, 4>(), static_cast<uint32_t>(command_status));
        serialize_u32(std::span{ buf }.subspan<12, 4>(), sequence_number);

        return buf;
    }
};
} // namespace pa::smpp