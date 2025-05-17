#pragma once

#include <pa/pinex/common.hpp>

namespace pa::pinex
{
struct bind_request
{
    pinex::bind_type bind_type{bind_type::bi_direction};
    std::string system_id{};

    bool operator==(const bind_request&) const = default;
};

namespace detail
{
inline auto command_id_of([[maybe_unused]] const bind_request& bind_request)
{
    return command_id::bind_req;
}

template<>
inline consteval auto members<bind_request>()
{
    using o = bind_request;
    return std::tuple{
        meta<c_octet_str<20>>(&o::system_id, "system_id")};
}
}  // namespace detail
}  // namespace pa::pinex