// descript

#pragma once

#include <cstdint>
#include <type_traits>

namespace descript {
    constexpr uint64_t dsHashFnv1a64(const uint8_t* bytes, uint32_t length, uint64_t hash = 0xcbf2'9ce4'8422'2325ull) noexcept
    {
        constexpr uint64_t prime = 0x0000'0100'0000'01b3ull;

        for (const uint8_t* const end = bytes + length; bytes != end; ++bytes)
        {
            const uint8_t value = *bytes;
            hash ^= value;
            hash *= prime;
        }

        return hash;
    }

    constexpr uint64_t dsHashFnv1a64(char const* start, char const* end = nullptr, uint64_t hash = 0xcbf2'9ce4'8422'2325ull) noexcept
    {
        constexpr uint64_t prime = 0x0000'0100'0000'01b3ull;

        if (end != nullptr)
        {
            for (; start != end; ++start)
            {
                const uint8_t value = static_cast<uint8_t>(*start);
                hash ^= value;
                hash *= prime;
            }
        }
        else
        {
            for (; *start != '\0'; ++start)
            {
                const uint8_t value = static_cast<uint8_t>(*start);
                hash ^= value;
                hash *= prime;
            }
        }

        return hash;
    }
} // namespace descript
