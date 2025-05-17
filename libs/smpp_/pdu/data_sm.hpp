#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct data_sm
{
    std::string service_type{};
    smpp::ton source_addr_ton{ ton::unknown };
    smpp::npi source_addr_npi{ npi::unknown };
    std::string source_addr{};
    smpp::ton dest_addr_ton{ ton::unknown };
    smpp::npi dest_addr_npi{ npi::unknown };
    std::string dest_addr{};
    smpp::esm_class esm_class{};
    smpp::registered_delivery registered_delivery;
    smpp::data_coding data_coding{ data_coding::defaults };
    smpp::oparam oparam{};

    bool operator==(const data_sm&) const = default;
};

namespace detail
{
inline auto command_id_of(const data_sm&)
{
    return command_id::data_sm;
}

template<>
inline consteval auto members<data_sm>()
{
    using o = data_sm;
    return std::tuple{ meta<c_octet_str<6>>(&o::service_type, "service_type"),
                       meta<enum_u8>(&o::source_addr_ton, "source_addr_ton"),
                       meta<enum_u8>(&o::source_addr_npi, "source_addr_npi"),
                       meta<c_octet_str<65>>(&o::source_addr, "source_addr"),
                       meta<enum_u8>(&o::dest_addr_ton, "dest_addr_ton"),
                       meta<enum_u8>(&o::dest_addr_npi, "dest_addr_npi"),
                       meta<c_octet_str<65>>(&o::dest_addr, "dest_addr"),
                       meta<enum_flag>(&o::esm_class, "esm_class"),
                       meta<enum_flag>(&o::registered_delivery, "registered_delivery"),
                       meta<enum_u8>(&o::data_coding, "data_coding"),
                       meta<smart>(&o::oparam, "oparam") };
}
} // namespace detail
} // namespace pa::smpp