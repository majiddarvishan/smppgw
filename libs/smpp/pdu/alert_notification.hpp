#pragma once

#include <smpp/common.hpp>
#include <smpp/param.hpp>

namespace pa::smpp
{
struct alert_notification
{
    smpp::ton source_addr_ton{ ton::unknown };
    smpp::npi source_addr_npi{ npi::unknown };
    std::string source_addr{};
    smpp::ton esme_addr_ton{ ton::unknown };
    smpp::npi esme_addr_npi{ npi::unknown };
    std::string esme_addr{};
    smpp::oparam oparam{};

    bool operator==(const alert_notification&) const = default;
};

namespace detail
{
inline auto command_id_of(const alert_notification&)
{
    return command_id::alert_notification;
}

template<>
inline consteval auto members<alert_notification>()
{
    using o = alert_notification;
    return std::tuple{ meta<enum_u8>(&o::source_addr_ton, "source_addr_ton"),
                       meta<enum_u8>(&o::source_addr_npi, "source_addr_npi"),
                       meta<c_octet_str<65>>(&o::source_addr, "source_addr"),
                       meta<enum_u8>(&o::esme_addr_ton, "esme_addr_ton"),
                       meta<enum_u8>(&o::esme_addr_npi, "esme_addr_npi"),
                       meta<c_octet_str<65>>(&o::esme_addr, "esme_addr"),
                       meta<smart>(&o::oparam, "oparam") };
}
} // namespace detail
} // namespace pa::smpp