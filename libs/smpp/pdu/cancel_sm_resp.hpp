#pragma once

#include <smpp/common.hpp>
#include <smpp/param.hpp>

namespace pa::smpp
{
struct cancel_sm_resp
{
    bool operator==(const cancel_sm_resp&) const = default;
};

namespace detail
{
inline auto command_id_of(const cancel_sm_resp&)
{
    return command_id::cancel_sm_resp;
}

template<>
inline constexpr auto is_response<cancel_sm_resp> = true;

template<>
inline consteval auto members<cancel_sm_resp>()
{
    return std::tuple{};
}
} // namespace detail
} // namespace pa::smpp