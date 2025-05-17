#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct query_sm_resp
{
    std::string message_id{};
    std::string final_date{};
    smpp::message_state message_state{ message_state::unknown };
    uint8_t error_code{};

    bool operator==(const query_sm_resp&) const = default;
};

namespace detail
{
inline auto command_id_of(const query_sm_resp&)
{
    return command_id::query_sm_resp;
}

template<>
inline constexpr auto is_response<query_sm_resp> = true;

template<>
inline consteval auto members<query_sm_resp>()
{
    using o = query_sm_resp;
    return std::tuple{ meta<c_octet_str<65>>(&o::message_id, "message_id"),
                       meta<c_octet_str<17>>(&o::final_date, "final_date"),
                       meta<enum_u8>(&o::message_state, "message_state"),
                       meta<u8>(&o::error_code, "error_code") };
}
} // namespace detail
} // namespace pa::smpp