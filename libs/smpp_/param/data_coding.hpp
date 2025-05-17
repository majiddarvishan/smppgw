#pragma once

#include <cinttypes>

namespace pa::smpp
{
enum class data_coding : uint8_t
{
    defaults = 0,
    ia5 = 1, // ia5 (ccitt t.50)/ascii (ansi x3.4)
    binary_alias = 2,
    iso8859_1 = 3, // latin 1
    binary = 4,
    jis = 5,
    iso8859_5 = 6, // cyrllic
    iso8859_8 = 7, // latin/hebrew
    ucs2 = 8,      // ucs-2be (big endian)
    pictogram = 9,
    iso2022_jp = 10, // music codes
    kanji = 13,      // extended kanji jis
    ksc5601 = 14
};
} // namespace pa::smpp