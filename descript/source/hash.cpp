// descript

#include "descript/hash.hh"

#include "fnv.hh"

namespace descript {
    uint64_t dsHash(uint8_t const* data, uint32_t length) noexcept { return dsHashFnv1a64(data, length); }
    uint64_t dsHash(uint8_t const* data, uint32_t length, uint64_t seed) noexcept { return dsHashFnv1a64(data, length, seed); }

    uint64_t dsHashString(char const* string, char const* stringEnd) noexcept { return dsHashFnv1a64(string, stringEnd); }
    uint64_t dsHashString(char const* string, char const* stringEnd, uint64_t seed) noexcept
    {
        return dsHashFnv1a64(string, stringEnd, seed);
    }
} // namespace descript
