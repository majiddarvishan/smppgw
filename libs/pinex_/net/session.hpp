#pragma once

#include <pa/pinex/common.hpp>
#include <pa/pinex/net/detail/flat_buffer.hpp>
#include <pa/pinex/pdu.hpp>

#include <boost/asio.hpp>

#include <memory>
#include <variant>
#include <optional>

namespace pa::pinex
{
using request = std::variant<std::monostate, bind_request, stream_request>;
using response = std::variant<std::monostate, bind_response, stream_response>;

class session : public std::enable_shared_from_this<session>
{
  public:
    std::function<void(std::optional<std::string>)> close_handler;                                               /* required */
    std::function<void(request&&, uint32_t)> request_handler;                                                    /* optional */
    std::function<void(response&&, uint32_t, command_status)> response_handler;                                  /* optional */
    std::function<void()> send_buf_available_handler;                                                            /* optional */
    std::function<void(const std::string&, command_id, std::span<const uint8_t>)> deserialization_error_handler; /* optional */

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

    static constexpr size_t header_length{10};

    state state_{state::open};
    receiving_state receiving_state_{receiving_state::paused};
    uint32_t sequence_number_{0};
    int inactivity_counter_{};
    boost::asio::ip::tcp::socket socket_;
    // boost::asio::steady_timer inactivity_timer_;
    std::vector<uint8_t> writing_send_buf_;
    std::vector<uint8_t> pending_send_buf_;
    size_t send_buf_threshold_{1024 * 1024};
    detail::flat_buffer<uint8_t, 1024 * 1024> receive_buf_{};

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

  public:
    explicit session(boost::asio::io_context* io_context, boost::asio::ip::tcp::socket socket)
      : socket_(std::move(socket))
      , strand_(boost::asio::make_strand(*io_context))
    //   , inactivity_timer_(socket_.get_executor())
    {
    }

    session(const session&) = delete;
    session& operator=(const session&) = delete;
    session(session&&) = delete;
    session& operator=(session&&) = delete;
    ~session() = default;

    void start()
    {
        // do_set_inactivity_timer();
        resume_receiving();
    }

    std::tuple<std::string, uint16_t> remote_endpoint() const
    {
        auto endpoint = socket_.remote_endpoint();
        return {endpoint.address().to_string(), endpoint.port()};
    }

    bool is_open() const
    {
        return state_ == state::open;
    }

    void unbind()
    {
        if (state_ == state::open)
        {
            state_ = state::unbinding;
            send_command(command_id::unbind_req);
        }
    }

    template<typename PDU>
    void send_response(const PDU& pdu, uint32_t sequence_number, command_status command_status)
    {
        static_assert(detail::is_response<PDU>, "PDU isn't a response");

        send_impl(pdu, sequence_number, command_status);
    }

    template<typename PDU>
    uint32_t send_request(const PDU& pdu)
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

        pause_receiving();

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

            handler(opt_error);

            auto sptr = wptr.lock();
            if(sptr)
            {
                sptr->request_handler = {};
                sptr->response_handler = {};
                sptr->send_buf_available_handler = {};
                sptr->close_handler = {};
                sptr->deserialization_error_handler = {};
            }
        });
    }

    uint32_t next_sequence_number()
    {
        if (++sequence_number_ > 0xFFFFFFFF)
            sequence_number_ = 1;
        return sequence_number_;
    }

    // void do_set_inactivity_timer()
    // {
    //     inactivity_timer_.expires_after(std::chrono::seconds{60});
    //     inactivity_timer_.async_wait([this, wptr = weak_from_this()](std::error_code ec) {
    //         if (wptr.expired())
    //             return;

    //         if (ec)
    //             return close(ec.message());

    //         if (inactivity_counter_ == 3)
    //             return close("Inactivity timer is reached");

    //         if (inactivity_counter_ >= 1 && state_ == state::open)
    //             send_command(command_id::enquire_link);

    //         inactivity_counter_++;

    //         do_set_inactivity_timer();
    //     });
    // }

    void do_receive()
    {
        while (true)
        {
            if (receiving_state_ != receiving_state::receiving)
                break;

            if (receive_buf_.size() < header_length)
                break;

            auto header_buf = std::span{receive_buf_}.subspan<0, header_length>();

            auto [command_length, command_id, command_status, sequence_number] = deserialize_header(header_buf);

            if (receive_buf_.size() < command_length)
                break;

            auto body_buf = std::span{receive_buf_.begin() + header_length, receive_buf_.begin() + command_length};

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

        socket_.async_receive(receive_buf_.prepare(64 * 1024),
            boost::asio::bind_executor(strand_,
                [this, wptr = weak_from_this()](boost::system::error_code ec, std::size_t received)
                {
                    if (ec)
                        return close(ec.message());

                    receive_buf_.commit(received);

                    if (state_ == state::open)
                        inactivity_counter_ = 0;

                    do_receive();
        }));

        // socket_.async_receive(receive_buf_.prepare(64 * 1024), [this, wptr = weak_from_this()](std::error_code ec, size_t received) {
        //     if (wptr.expired())
        //         return;

        //     if (ec)
        //         return close(ec.message());

        //     receive_buf_.commit(received);

        //     if (state_ == state::open)
        //         inactivity_counter_ = 0;

        //     do_receive();
        // });
    }

    void consume_response_pdu(command_id command_id, command_status command_status, uint32_t sequence_number, std::span<const uint8_t> buf)
    {
        response resp;

        try
        {
            switch (command_id)
            {
                case command_id::enquire_link_resp:
                    break;
                case command_id::unbind_resp:
                    close("unbind_resp received");
                    break;
                case command_id::bind_resp:
                    resp = deserialize<bind_response>(buf, bind_type::bi_direction);
                    break;
                case command_id::stream_resp:
                    resp = deserialize<stream_response>(buf);
                    break;
                default:
                    throw std::logic_error{"Unknown pdu"};
            }
        }
        catch (const std::exception& ex)
        {
            if (deserialization_error_handler)
                deserialization_error_handler(std::string{ex.what()}, command_id, buf);

            close("an exeception is occured");
        }

        if (resp.index() != 0 && state_ == state::open && response_handler)
            response_handler(std::move(resp), sequence_number, command_status);
    }

    void consume_request_pdu(command_id command_id, command_status /* command_status */, uint32_t sequence_number, std::span<const uint8_t> buf)
    {
        request req;

        try
        {
            switch (command_id)
            {
                case command_id::enquire_link_req:
                    send_command(command_id::enquire_link_resp, sequence_number);
                    break;
                case command_id::unbind_req:
                    if (state_ == state::open)
                        state_ = state::unbinding;
                    send_command(command_id::unbind_resp, sequence_number);
                    break;
                case command_id::bind_req:
                    req = deserialize<bind_request>(buf, bind_type::bi_direction);
                    break;
                case command_id::stream_req:
                    req = deserialize<stream_request>(buf);
                    break;
                default:
                    throw std::logic_error{"Unknown pdu"};
            }
        }
        catch (const std::exception& ex)
        {
            if (deserialization_error_handler)
                deserialization_error_handler(std::string{ex.what()}, command_id, buf);

            close("an exeception is occured");
        }

        if (req.index() != 0 && state_ == state::open && request_handler)
            request_handler(std::move(req), sequence_number);
    }

    template<typename PDU>
    void send_impl(const PDU& pdu, uint32_t sequence_number, command_status command_status)
    {
        if (state_ == state::close)
            throw std::logic_error{"Send on closed session"};

        if (state_ == state::unbinding)
            throw std::logic_error{"Send on unbinded session"};

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

        auto header = serialize_header(command_length, detail::command_id_of(pdu), sequence_number, command_status);

        std::copy(header.begin(), header.end(), pending_send_buf_.begin() + static_cast<std::ptrdiff_t>(prev_size));

        do_send();
    }

    uint32_t send_command(command_id command_id)
    {
        auto sequence_number = next_sequence_number();

        send_command(command_id, sequence_number);

        return sequence_number;
    }

    void send_command(command_id command_id, uint32_t sequence_number, command_status command_status = command_status::rok)
    {
        auto header = serialize_header(header_length, command_id, sequence_number, command_status);

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
            send_buf_available_handler();

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
        return static_cast<uint8_t>(command_id) & 0x80;
    }

    static std::tuple<uint32_t, command_id, command_status, uint32_t> deserialize_header(std::span<const uint8_t, header_length> buf)
    {
        auto deserialize_u32 = [](std::span<const uint8_t, 4> b) -> uint32_t {
            return b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3];
        };

        auto command_length = deserialize_u32(buf.subspan<0, 4>());
        // auto command_id = static_cast<pinex::command_id>(deserialize_u32(buf.subspan<4, 4>()));
        auto command_id = static_cast<pinex::command_id>(buf[4]);
        // auto command_status = static_cast<pinex::command_status>(deserialize_u32(buf.subspan<8, 4>()));
        auto command_status = static_cast<pinex::command_status>(buf[5]);
        auto sequence_number = deserialize_u32(buf.subspan<6, 4>());

        return {command_length, command_id, command_status, sequence_number};
    }

    static std::array<uint8_t, header_length> serialize_header(uint32_t command_length, command_id command_id, uint32_t sequence_number, command_status command_status)
    {
        std::array<uint8_t, header_length> buf{};

        auto serialize_u32 = [](std::span<uint8_t, 4> b, uint32_t val) {
            b[0] = (val >> 24) & 0xFF;
            b[1] = (val >> 16) & 0xFF;
            b[2] = (val >> 8) & 0xFF;
            b[3] = (val >> 0) & 0xFF;
        };

        serialize_u32(std::span{buf}.subspan<0, 4>(), command_length);
        // serialize_u32(std::span{buf}.subspan<4, 4>(), static_cast<uint32_t>(command_id));
        // serialize_u32(std::span{buf}.subspan<8, 4>(), static_cast<uint32_t>(command_status));
        buf[4] = static_cast<uint8_t>(command_id);
        buf[5] = static_cast<uint8_t>(command_status);
        serialize_u32(std::span{buf}.subspan<6, 4>(), sequence_number);

        return buf;
    }
};
}  // namespace pa::pinex