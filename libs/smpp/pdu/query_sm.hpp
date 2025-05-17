#pragma once

#include <smpp/common.hpp>
#include <smpp/param.hpp>

namespace pa::smpp
{
struct query_sm
{
    std::string message_id{};
    smpp::ton source_addr_ton{ ton::unknown };
    smpp::npi source_addr_npi{ npi::unknown };
    std::string source_addr{};

    bool operator==(const query_sm&) const = default;
};

namespace detail
{
inline auto command_id_of(const query_sm&)
{
    return command_id::query_sm;
}

template<>
inline consteval auto members<query_sm>()
{
    using o = query_sm;
    return std::tuple{ meta<c_octet_str<65>>(&o::message_id, "message_id"),
                       meta<enum_u8>(&o::source_addr_ton, "source_addr_ton"),
                       meta<enum_u8>(&o::source_addr_npi, "source_addr_npi"),
                       meta<c_octet_str<21>>(&o::source_addr, "source_addr") };
}
} // namespace detail
} // namespace pa::smpp