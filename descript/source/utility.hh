// descript

#pragma once

#include "descript/types.hh"

#include "assert.hh"

#include <cstdint>
#include <cstring>

namespace descript {
    constexpr uint32_t dsAlign(uint32_t value, uint32_t alignment) noexcept
    {
        uint32_t const mask = alignment - 1;
        uint32_t const overage = value & mask;
        return overage == 0 ? value : value + (alignment - overage);
    }

    template <typename T, uint32_t Count>
    constexpr uint32_t dsCountOf(T (&arry)[Count]) noexcept
    {
        return Count;
    }

    template <typename ValueT, typename IndexT = uint32_t>
    class dsEnumerated
    {
    public:
        struct Entry
        {
            IndexT index = 0;
            ValueT& item;
        };

        class Iterator
        {
        public:
            constexpr Iterator(ValueT* items, IndexT index) noexcept : item_(items), index_(index) {}

            constexpr Iterator& operator++() noexcept
            {
                ++index_;
                ++item_;
                return *this;
            }

            constexpr Entry operator*() const noexcept { return {.index = index_, .item = *item_}; }

            constexpr bool operator==(Iterator const&) const noexcept = default;

        private:
            ValueT* item_ = nullptr;
            uint32_t index_{};
        };

        constexpr dsEnumerated(ValueT* items, uint32_t size) noexcept : items_(items), size_(size) {}

        constexpr Iterator begin() const noexcept { return Iterator(items_, 0); }
        constexpr Iterator end() const noexcept { return Iterator(items_ + size_, size_); }

    private:
        ValueT* items_ = nullptr;
        uint32_t size_ = 0;
    };

    template <typename T>
    struct dsSpan
    {
        dsSpan() noexcept = default;
        template <size_t N>
        dsSpan(T (&arr)[N]) noexcept : items(arr), count(N)
        {
        }
        dsSpan(T* start, T* end) noexcept : items(start), count(end - start) {}

        T* items = nullptr;
        uint32_t count = 0;

        T* begin() const noexcept { return items; }
        T* end() const noexcept { return items + count; }
    };

    template <typename ContainerT>
    constexpr auto dsEnumerate(ContainerT& container) noexcept
    {
        return dsEnumerated{container.data(), container.size()};
    }

    constexpr uint32_t dsNameLen(dsName name) noexcept
    {
        if (name.name == nullptr)
            return 0;

        if (name.nameEnd != nullptr)
            return name.nameEnd - name.name;

        return std::strlen(name.name);
    }

    constexpr bool dsIsNameEmpty(dsName name) noexcept
    {
        if (name.name == nullptr)
            return true;

        if (name.nameEnd != nullptr && name.nameEnd == name.name)
            return true;

        return name.name[0] == '\0';
    }
} // namespace descript

#define DS_GUARD_OR(x, r, ...) \
    if (DS_VERIFY(x))          \
    {                          \
    }                          \
    else                       \
    {                          \
        DS_BREAK();            \
        return (r);            \
    }

#define DS_GUARD_VOID(x, ...) \
    if (DS_VERIFY(x))         \
    {                         \
    }                         \
    else                      \
    {                         \
        DS_BREAK();           \
        return;               \
    }
