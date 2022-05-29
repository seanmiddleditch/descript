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

    DS_EXTRA_API dsUuid dsCreateUuid() noexcept;
    DS_EXTRA_API dsStringUuid dsUuidToString(dsUuid const& uuid) noexcept;
    DS_EXTRA_API dsUuid dsParseUuid(char const* string, char const* stringEnd) noexcept;

    constexpr dsUuid dsParseUuid(char const* string) noexcept
    {
        if (string == nullptr)
            return dsUuid{};

        if (*string == '{')
            ++string;

        dsUuid result;
        uint32_t nibbleIndex = 0;
        while (*string != '\0' && nibbleIndex != (dsUuid::length << 1))
        {
            char const ch = *string++;
            if (ch == '-')
                continue;

            int const value = (ch >= '0' && ch <= '9')   ? ch - '0'
                              : (ch >= 'a' && ch <= 'f') ? 10 + (ch - 'a')
                              : (ch >= 'A' && ch <= 'F') ? 10 + (ch - 'A')
                                                         : -1;
            if (value == -1)
                return dsUuid{};

            if ((nibbleIndex & 1) == 0)
                result.bytes[nibbleIndex++ >> 1] = (value << 4);
            else
                result.bytes[nibbleIndex++ >> 1] |= value;
        }

        if (*string == '}')
            ++string;

        if (*string != '\0')
            return dsUuid{};

        return result;
    }
} // namespace descript
