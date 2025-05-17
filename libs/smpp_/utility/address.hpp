#pragma once

#include <pa/smpp/param/ton.hpp>

#include <string>
#include <string_view>

namespace pa::smpp
{
inline std::string convert_to_international(smpp::ton ton, std::string_view addr)
{
    const std::string country_code = "98";

    if (addr.empty())
        return std::string{ addr };

    if (ton == ton::unknown)
    {
        if (addr[0] == '0')
        {
            if (addr[1] == '0')
            {
                return std::string{ addr.substr(2) };
            }

            return country_code + std::string{ addr.substr(1) };
        }

        if (addr.substr(0, country_code.length()) != country_code)
        {
            return country_code + std::string{ addr };
        }
    }
    else if (ton == ton::national)
    {
        return country_code + std::string{ addr };
    }

    return std::string{ addr };
}
} // namespace pa::smpp