#pragma once

#include <cinttypes>

namespace pa::smpp
{
enum class ton : uint8_t
{
    unknown = 0x00,
    international = 0x01,
    national = 0x02,
    networkspecific = 0x03,
    subscribernumber = 0x04,
    alphanumeric = 0x05,
    abbreviated = 0x06
};
} // namespace pa::smpp