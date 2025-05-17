#pragma once

#include <cinttypes>

namespace pa::smpp
{
enum class npi : uint8_t
{
    unknown = 0x00,
    e164 = 0x01,
    data = 0x03,
    telex = 0x04,
    e212 = 0x06,
    national = 0x08,
    privates = 0x09,
    ermes = 0x0a,
    internet = 0x0e,
    wapclient = 0x12
};
} // namespace pa::smpp