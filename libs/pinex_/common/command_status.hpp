#pragma once

#include <cinttypes>

namespace pa::pinex
{
enum class command_status : uint8_t
{
    rok = 0x00,    // no error
    rfail = 0xFF,  // failed
};
}
