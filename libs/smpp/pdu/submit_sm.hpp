#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct submit_sm
{
    std::string service_type{};
    smpp::ton source_addr_ton{ ton::unknown };
    smpp::npi source_addr_npi{ npi::unknown };
    std::string source_addr{};
    smpp::ton dest_addr_ton{ ton::unknown };
    smpp::npi dest_addr_npi{ npi::unknown };
    std::string dest_addr{};
    smpp::esm_class esm_class{};
    uint8_t protocol_id{};
    smpp::priority_flag priority_flag{ priority_flag::gsm_non_priority };
    std::string schedule_delivery_time{};
    std::string validity_period{};
    smpp::registered_delivery registered_delivery{};
    smpp::replace_if_present_flag replace_if_present_flag{ replace_if_present_flag::no };
    smpp::data_coding data_coding{ data_coding::defaults };
    uint8_t sm_default_msg_id{};
    std::string short_message{};
    smpp::oparam oparam{};

    bool operator==(const submit_sm&) const = default;
};

namespace detail
{
inline auto command_id_of(const submit_sm&)
{
    return command_id::submit_sm;
}

template<>
inline consteval auto members<submit_sm>()
{
    using o = submit_sm;
    return std::tuple{ meta<c_octet_str<6>>(&o::service_type, "service_type"),
                       meta<enum_u8>(&o::source_addr_ton, "source_addr_ton"),
                       meta<enum_u8>(&o::source_addr_npi, "source_addr_npi"),
                       meta<c_octet_str<21>>(&o::source_addr, "source_addr"),
                       meta<enum_u8>(&o::dest_addr_ton, "dest_addr_ton"),
                       meta<enum_u8>(&o::dest_addr_npi, "dest_addr_npi"),
                       meta<c_octet_str<21>>(&o::dest_addr, "dest_addr"),
                       meta<enum_flag>(&o::esm_class, "esm_class"),
                       meta<u8>(&o::protocol_id, "protocol_id"),
                       meta<enum_u8>(&o::priority_flag, "priority_flag"),
                       meta<c_octet_str<17>>(&o::schedule_delivery_time, "schedule_delivery_time"),
                       meta<c_octet_str<17>>(&o::validity_period, "validity_period"),
                       meta<enum_flag>(&o::registered_delivery, "registered_delivery"),
                       meta<enum_u8>(&o::replace_if_present_flag, "replace_if_present_flag"),
                       meta<enum_u8>(&o::data_coding, "data_coding"),
                       meta<u8>(&o::sm_default_msg_id, "sm_default_msg_id"),
                       meta<u8_octet_str<254>>(&o::short_message, "short_message"),
                       meta<smart>(&o::oparam, "oparam") };
}
} // namespace detail
} // namespace pa::smpp