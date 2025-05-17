#pragma once

#include <cinttypes>

namespace pa::pinex
{
enum class command_id : uint8_t
{
    bind_req = 0x01,
    bind_resp = 0x81,
    stream_req = 0x02,
    stream_resp = 0x82,
    unbind_req = 0x03,
    unbind_resp = 0x83,
    enquire_link_req = 0x04,
    enquire_link_resp = 0x84,
};
}