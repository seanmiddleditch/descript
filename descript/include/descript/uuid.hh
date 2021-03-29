// descript

#pragma once

#include "descript/export.hh"

#include <compare>
#include <cstdint>

namespace descript {

    struct dsUuid final
    {
        constexpr dsUuid() = default;

        static constexpr uint32_t length = 16;

        uint8_t bytes[length] = {};

        constexpr bool valid() const noexcept;
        constexpr explicit operator bool() const noexcept { return valid(); }

        constexpr bool operator==(const dsUuid&) const = default;
        constexpr std::strong_ordering operator<=>(const dsUuid&) const = default;
    };

    struct dsStringUuid final
    {
        // {8-4-4-4-12} format
        // 32 digits, 4 dashes, 2 braces, 1 nul byte
        char string[39] = {};
    };

    constexpr bool dsUuid::valid() const noexcept
    {
        for (uint32_t index = 0; index != length; ++index)
            if (bytes[index] != 0)
                return true;
        return false;
    }

    DS_API dsUuid dsCreateUuid() noexcept;
    DS_API dsStringUuid dsUuidToString(dsUuid const& uuid) noexcept;
    constexpr dsUuid dsParseUuid(char const* string) noexcept;

    constexpr dsUuid dsParseUuid(char const* string) noexcept
    {
        dsUuid result;

        if (string == nullptr)
            return result;

        uint32_t nibbleIndex = 0;
        while (*string != '\0' && nibbleIndex != (dsUuid::length << 1))
        {
            char const ch = *string++;
            int const value = (ch >= '0' && ch <= '9')   ? ch - '0'
                              : (ch >= 'a' && ch <= 'z') ? 10 + (ch - 'a')
                              : (ch >= 'A' && ch <= 'Z') ? 10 + (ch - 'A')
                                                         : -1;
            if (value != -1)
            {
                if ((nibbleIndex & 1) == 0)
                    result.bytes[nibbleIndex++ >> 1] = (value << 4);
                else
                    result.bytes[nibbleIndex++ >> 1] |= value;
            }
        }

        return result;
    }
} // namespace descript
