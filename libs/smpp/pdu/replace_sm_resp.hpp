#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct replace_sm_resp
{
    bool operator==(const replace_sm_resp&) const = default;
};

namespace detail
{
inline auto command_id_of(const replace_sm_resp&)
{
    return command_id::replace_sm_resp;
}

template<>
inline constexpr auto is_response<replace_sm_resp> = true;

template<>
inline consteval auto members<replace_sm_resp>()
{
    return std::tuple{};
}
} // namespace detail
} // namespace pa::smpp