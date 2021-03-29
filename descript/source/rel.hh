// descript

#pragma once

#include "assert.hh"
#include "utility.hh"

#include <cstdint>

namespace descript {
    template <typename T>
    struct dsRelativeObject
    {
        uint32_t offset = 0;

        constexpr explicit operator bool() const noexcept { return offset != 0; }

        T const* get() const noexcept
        {
            DS_ASSERT(offset != 0);
            return reinterpret_cast<T const*>(reinterpret_cast<uintptr_t>(this) + offset);
        }

        T* get() noexcept
        {
            DS_ASSERT(offset != 0);
            return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(this) + offset);
        }

        T const& operator*() const noexcept { return *get(); }

        T const* operator->() const noexcept { return get(); }

        static uint32_t allocate(uint32_t& size) noexcept
        {
            size = dsAlign(size, alignof(T));
            uint32_t const start = size;
            size += sizeof(T);
            return size;
        }

        void assign(uintptr_t block, uint32_t offset) noexcept
        {
            uintptr_t const self = reinterpret_cast<uintptr_t>(this);
            this->offset = offset - (self - block);
        }

        bool validate(uintptr_t block, uint32_t size) const noexcept
        {
            uintptr_t const start = reinterpret_cast<uintptr_t>(this) + offset;
            uintptr_t const end = start + sizeof(T);
            return start < end && block <= start && end <= block + size;
        }
    };

    template <typename T, typename IndexT = uint32_t>
    struct dsRelativeArray
    {
        dsRelativeObject<T> base;
        uint32_t count = 0;

        constexpr explicit operator bool() const noexcept { return count != 0 && base; }

        T* data() noexcept { return base.get(); }
        T const* data() const noexcept { return base.get(); }

        T const& operator[](IndexT index) const noexcept
        {
            DS_ASSERT(static_cast<uint32_t>(index) < count);
            return base.get()[static_cast<uint32_t>(index)];
        }

        T& operator[](IndexT index) noexcept
        {
            DS_ASSERT(static_cast<uint32_t>(index) < count);
            return base.get()[static_cast<uint32_t>(index)];
        }

        T const* begin() const noexcept { return base.get(); }
        T const* end() const noexcept { return base.get() + count; }

        T* begin() noexcept { return base.get(); }
        T* end() noexcept { return base.get() + count; }

        static uint32_t allocate(uint32_t& size, uint32_t length) noexcept
        {
            uint32_t start = 0;
            if (length != 0)
            {
                size = dsAlign(size, alignof(T));
                start = size;
                size += sizeof(T) * length;
            }
            return start;
        }

        void assign(uintptr_t block, uint32_t offset, uint32_t length) noexcept
        {
            base.assign(block, offset);
            count = length;
        }

        bool validate(uintptr_t block, uint32_t size) const noexcept
        {
            uintptr_t const start = reinterpret_cast<uintptr_t>(this) + base.offset;
            uintptr_t const end = start + count * sizeof(T);
            return start <= end && block <= start && end <= block + size;
        }
    };

    template <typename IndexT = uint32_t>
    struct dsRelativeBitArray
    {
        dsRelativeObject<uint64_t> base;
        uint32_t count = 0;

        static constexpr uint32_t bitShift = 6;
        static constexpr uint64_t bitWidth = 64;
        static constexpr uint64_t bitMask = bitWidth - 1;

        constexpr explicit operator bool() const noexcept { return count != 0 && base; }

        bool operator[](IndexT index) const noexcept
        {
            uint32_t const rawIndex = static_cast<uint32_t>(index);
            DS_ASSERT(rawIndex < count);
            uint64_t const* bytes = base.get();
            return bytes[rawIndex >> bitShift] & (1ul << (rawIndex & bitMask));
        }

        void set(IndexT index) noexcept
        {
            uint32_t const rawIndex = static_cast<uint32_t>(index);
            DS_ASSERT(rawIndex < count);
            uint64_t* bytes = base.get();
            bytes[rawIndex >> bitShift] |= (1ull << (rawIndex & bitMask));
        }

        void clear(IndexT index) noexcept
        {
            uint32_t const rawIndex = static_cast<uint32_t>(index);
            DS_ASSERT(rawIndex < count);
            uint64_t* bytes = base.get();
            bytes[rawIndex >> bitShift] &= ~(1ull << (rawIndex & bitMask));
        }

        static uint32_t allocate(uint32_t& size, uint32_t length) noexcept
        {
            uint32_t start = 0;
            if (length != 0)
            {
                uint32_t const byteLength = dsAlign(length, bitWidth) >> bitShift;
                size = dsAlign(size, alignof(uint64_t));
                start = size;
                size += sizeof(uint64_t) * byteLength;
            }
            return start;
        }

        void assign(uintptr_t block, uint32_t offset, uint32_t length) noexcept
        {
            base.assign(block, offset);
            count = length;
        }

        bool validate(uintptr_t block, uint32_t size) const noexcept
        {
            uint32_t const byteLength = dsAlign(count, bitWidth) >> bitShift;
            uintptr_t const start = reinterpret_cast<uintptr_t>(this) + base.offset;
            uintptr_t const end = start + byteLength;
            return start <= end && block <= start && end <= block + size;
        }
    };
} // namespace descript
