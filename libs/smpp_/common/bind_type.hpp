#pragma once

namespace pa::smpp
{
enum class bind_type
{
    receiver = 0x01,
    transmitter = 0x02,
    transceiver = 0x09
};
} // namespace pa::smpp
