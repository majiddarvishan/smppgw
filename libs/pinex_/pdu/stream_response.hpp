#pragma once

#include <pa/pinex/common.hpp>

namespace pa::pinex
{
struct stream_response
{
    std::string message_body{};

    bool operator==(const stream_response&) const = default;
};

namespace detail
{
inline auto command_id_of([[maybe_unused]] const stream_response&)
{
    return command_id::stream_resp;
}

template<>
inline constexpr auto is_response<stream_response> = true;

template<>
inline constexpr auto can_be_omitted<stream_response> = true;

template<>
inline consteval auto members<stream_response>()
{
    using o = stream_response;
    return std::tuple{meta<octet_str>(&o::message_body, "message_body")};
}
}  // namespace detail
}  // namespace pa::pinex