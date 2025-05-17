#pragma once

#include <pa/pinex/common.hpp>

namespace pa::pinex
{
struct bind_response
{
    pinex::bind_type bind_type{bind_type::bi_direction};
    std::string system_id{};

    bool operator==(const bind_response&) const = default;
};

namespace detail
{
inline auto command_id_of([[maybe_unused]] const bind_response& bind_response)
{
    return command_id::bind_resp;
}

template<>
inline constexpr auto is_response<bind_response> = true;

template<>
inline constexpr auto can_be_omitted<bind_response> = true;

template<>
inline consteval auto members<bind_response>()
{
    using o = bind_response;
    return std::tuple{meta<c_octet_str<20>>(&o::system_id, "system_id")};
}
}  // namespace detail
}  // namespace pa::pinex