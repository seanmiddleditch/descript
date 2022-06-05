// descript

#pragma once

#include "descript/export.hh"

#include <cstdint>

namespace descript {
    DS_API [[nodiscard]] uint64_t dsHash(uint8_t const* data, uint32_t length) noexcept;
    DS_API [[nodiscard]] uint64_t dsHash(uint8_t const* data, uint32_t length, uint64_t seed) noexcept;

    DS_API [[nodiscard]] uint64_t dsHashString(char const* string, char const* stringEnd) noexcept;
    DS_API [[nodiscard]] uint64_t dsHashString(char const* string, char const* stringEnd, uint64_t seed) noexcept;
} // namespace descript
