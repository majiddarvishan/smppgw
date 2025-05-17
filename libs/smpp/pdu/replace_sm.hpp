#pragma once

#include <smpp/common.hpp>
#include <smpp/param.hpp>

namespace pa::smpp
{
struct replace_sm
{
    std::string message_id{};
    smpp::ton source_addr_ton{ ton::unknown };
    smpp::npi source_addr_npi{ npi::unknown };
    std::string source_addr{};
    std::string schedule_delivery_time{};
    std::string validity_period{};
    smpp::registered_delivery registered_delivery{};
    uint8_t sm_default_msg_id{};
    std::string short_message{};

    bool operator==(const replace_sm&) const = default;
};

namespace detail
{
inline auto command_id_of(const replace_sm&)
{
    return command_id::replace_sm;
}

template<>
inline consteval auto members<replace_sm>()
{
    using o = replace_sm;
    return std::tuple{ meta<c_octet_str<65>>(&o::message_id, "message_id"),
                       meta<enum_u8>(&o::source_addr_ton, "source_addr_ton"),
                       meta<enum_u8>(&o::source_addr_npi, "source_addr_npi"),
                       meta<c_octet_str<21>>(&o::source_addr, "source_addr"),
                       meta<c_octet_str<17>>(&o::schedule_delivery_time, "schedule_delivery_time"),
                       meta<c_octet_str<17>>(&o::validity_period, "validity_period"),
                       meta<enum_flag>(&o::registered_delivery, "registered_delivery"),
                       meta<u8>(&o::sm_default_msg_id, "sm_default_msg_id"),
                       meta<u8_octet_str<254>>(&o::short_message, "short_message") };
}
} // namespace detail
} // namespace pa::smpp