#pragma once

#include <cinttypes>

namespace pa::smpp
{
enum class message_state : uint8_t
{
    enroute = 1,
    delivered = 2,
    expired = 3,
    deleted = 4,
    undeliverable = 5,
    accepted = 6,
    unknown = 7,
    rejected = 8
};
} // namespace pa::smpp