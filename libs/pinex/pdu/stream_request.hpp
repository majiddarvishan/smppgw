#pragma once

#include <pinex/common.hpp>

namespace pa::pinex
{
struct stream_request
{
    std::string message_body{};

    bool operator==(const stream_request&) const = default;
};

namespace detail
{
inline auto command_id_of([[maybe_unused]] const stream_request&)
{
    return command_id::stream_req;
}

template<>
inline consteval auto members<stream_request>()
{
    using o = stream_request;
    return std::tuple{meta<octet_str>(&o::message_body, "message_body")};
}
}  // namespace detail
}  // namespace pa::pinex