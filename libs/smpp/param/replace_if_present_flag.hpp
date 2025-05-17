#pragma once

#include <cinttypes>

namespace pa::smpp
{
enum class replace_if_present_flag : uint8_t
{
    no = 0x00,
    yes = 0x01
};
} // namespace pa::smpp