#pragma once

#include <smpp/common.hpp>
#include <smpp/param.hpp>

namespace pa::smpp
{
struct data_sm_resp
{
    std::string message_id{};
    smpp::oparam oparam{};

    bool operator==(const data_sm_resp&) const = default;
};

namespace detail
{
inline auto command_id_of(const data_sm_resp&)
{
    return command_id::data_sm_resp;
}

template<>
inline constexpr auto is_response<data_sm_resp> = true;

template<>
inline consteval auto members<data_sm_resp>()
{
    using o = data_sm_resp;
    return std::tuple{ meta<c_octet_str<65>>(&o::message_id, "message_id"), meta<smart>(&o::oparam, "oparam") };
}
} // namespace detail
} // namespace pa::smpp