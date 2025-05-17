#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct deliver_sm_resp
{
    std::string message_id{};

    bool operator==(const deliver_sm_resp&) const = default;
};

namespace detail
{
inline auto command_id_of(const deliver_sm_resp&)
{
    return command_id::deliver_sm_resp;
}

template<>
inline constexpr auto is_response<deliver_sm_resp> = true;

template<>
inline constexpr auto can_be_omitted<deliver_sm_resp> = true;

template<>
inline consteval auto members<deliver_sm_resp>()
{
    using o = deliver_sm_resp;
    return std::tuple{ meta<c_octet_str<1>>(&o::message_id, "message_id") };
}
} // namespace detail
} // namespace pa::smpp