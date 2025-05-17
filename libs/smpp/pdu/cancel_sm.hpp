#pragma once

#include <smpp/common.hpp>
#include <smpp/param.hpp>

namespace pa::smpp
{
struct cancel_sm
{
    std::string service_type{};
    std::string message_id{};
    smpp::ton source_addr_ton{ ton::unknown };
    smpp::npi source_addr_npi{ npi::unknown };
    std::string source_addr{};
    smpp::ton dest_addr_ton{ ton::unknown };
    smpp::npi dest_addr_npi{ npi::unknown };
    std::string dest_addr{};

    bool operator==(const cancel_sm&) const = default;
};

namespace detail
{
inline auto command_id_of(const cancel_sm&)
{
    return command_id::cancel_sm;
}

template<>
inline consteval auto members<cancel_sm>()
{
    using o = cancel_sm;
    return std::tuple{ meta<c_octet_str<6>>(&o::service_type, "service_type"), meta<c_octet_str<65>>(&o::message_id, "message_id"),
                       meta<enum_u8>(&o::source_addr_ton, "source_addr_ton"),  meta<enum_u8>(&o::source_addr_npi, "source_addr_npi"),
                       meta<c_octet_str<21>>(&o::source_addr, "source_addr"),  meta<enum_u8>(&o::dest_addr_ton, "dest_addr_ton"),
                       meta<enum_u8>(&o::dest_addr_npi, "dest_addr_npi"),      meta<c_octet_str<21>>(&o::dest_addr, "dest_addr") };
}
} // namespace detail
} // namespace pa::smpp