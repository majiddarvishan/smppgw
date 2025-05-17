#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct bind_resp
{
    smpp::bind_type bind_type{ bind_type::transceiver };
    std::string system_id{};
    smpp::oparam oparam{};

    bool operator==(const bind_resp&) const = default;
};

namespace detail
{
inline auto command_id_of(const bind_resp& bind_resp)
{
    if (bind_resp.bind_type == bind_type::transmitter)
        return command_id::bind_transmitter_resp;

    if (bind_resp.bind_type == bind_type::receiver)
        return command_id::bind_receiver_resp;

    return command_id::bind_transceiver_resp;
}

template<>
inline constexpr auto is_response<bind_resp> = true;

template<>
inline constexpr auto can_be_omitted<bind_resp> = true;

template<>
inline consteval auto members<bind_resp>()
{
    using o = bind_resp;
    return std::tuple{
        meta<c_octet_str<31>>(&o::system_id, "system_id"),
        meta<smart>(&o::oparam, "oparam") };
}
} // namespace detail
} // namespace pa::smpp