#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct submit_sm_resp
{
    std::string message_id{};

    bool operator==(const submit_sm_resp&) const = default;
};

namespace detail
{
inline auto command_id_of(const submit_sm_resp&)
{
    return command_id::submit_sm_resp;
}

template<>
inline constexpr auto is_response<submit_sm_resp> = true;

template<>
inline constexpr auto can_be_omitted<submit_sm_resp> = true;

template<>
inline consteval auto members<submit_sm_resp>()
{
    using o = submit_sm_resp;
    return std::tuple{ meta<c_octet_str<65>>(&o::message_id, "message_id") };
}
} // namespace detail
} // namespace pa::smpp