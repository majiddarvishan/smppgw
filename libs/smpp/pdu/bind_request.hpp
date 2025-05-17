#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct bind_request
{
    smpp::bind_type bind_type{ bind_type::transceiver };
    std::string system_id{};
    std::string password{};
    std::string system_type{};
    smpp::interface_version interface_version{ interface_version::smpp_3_4 };
    smpp::ton addr_ton{ ton::unknown };
    smpp::npi addr_npi{ npi::unknown };
    std::string address_range{};

    bool operator==(const bind_request&) const = default;
};

namespace detail
{
inline auto command_id_of(const bind_request& bind_request)
{
    if (bind_request.bind_type == bind_type::transmitter)
        return command_id::bind_transmitter;

    if (bind_request.bind_type == bind_type::receiver)
        return command_id::bind_receiver;

    return command_id::bind_transceiver;
}

template<>
inline consteval auto members<bind_request>()
{
    using o = bind_request;
    return std::tuple{
        meta<c_octet_str<31>>(&o::system_id, "system_id"),
        meta<c_octet_str<31>>(&o::password, "password"),
        meta<c_octet_str<13>>(&o::system_type, "system_type"),
        meta<enum_u8>(&o::interface_version, "interface_version"),
        meta<enum_u8>(&o::addr_ton, "addr_ton"),
        meta<enum_u8>(&o::addr_npi, "addr_npi"),
        meta<c_octet_str<41>>(&o::address_range, "address_range")
    };
}
} // namespace detail
} // namespace pa::smpp