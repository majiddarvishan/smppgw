#pragma once

#include <cinttypes>

namespace pa::smpp
{
enum class priority_flag : uint8_t
{
    gsm_non_priority = 0x0,
    gsm_priority = 0x1,
    ansi_136_bulk = 0x0,
    ansi_136_normal = 0x1,
    ansi_136_urgent = 0x2,
    ansi_136_very_urgent = 0x3,
    is_95_normal = 0x0,
    is_95_interactive = 0x1,
    is_95_urgent = 0x2,
    is_95_emergency = 0x3
};
} // namespace pa::smpp