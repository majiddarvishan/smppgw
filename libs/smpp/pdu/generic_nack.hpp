#pragma once

#include <pa/smpp/common.hpp>
#include <pa/smpp/param.hpp>

namespace pa::smpp
{
struct generic_nack
{
    bool operator==(const generic_nack&) const = default;
};

namespace detail
{
inline auto command_id_of(const generic_nack&)
{
    return command_id::generic_nack;
}

template<>
inline constexpr auto is_response<generic_nack> = true;

template<>
inline consteval auto members<generic_nack>()
{
    return std::tuple{};
}
} // namespace detail
} // namespace pa::smpp