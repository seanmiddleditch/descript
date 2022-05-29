// descript

#pragma once

#include <compare>
#include <cstdint>

#include "descript/types.hh"

namespace descript {
    using dsTypeOpEquality = bool (*)(void const* left, void const* right) noexcept;
    using dsTypeOpCopyTo = void (*)(void* dest, void const* source) noexcept;

    struct dsTypeMeta final
    {
        char const* name = nullptr;
        uint32_t typeId = 0;
        uint32_t size = 0;
        uint32_t align = 0;
        dsTypeOpEquality opEquality = nullptr;
        dsTypeOpCopyTo opCopyTo = nullptr;
    };

    template <typename T>
    struct dsValueTraits;

    template <>
    struct dsValueTraits<decltype(nullptr)>
    {
        static constexpr char name[] = "nil";
    };

    template <>
    struct dsValueTraits<float>
    {
        static constexpr char name[] = "float32";
    };

    template <>
    struct dsValueTraits<int32_t>
    {
        static constexpr char name[] = "int32";
    };

    template <>
    struct dsValueTraits<bool>
    {
        static constexpr char name[] = "bool";
    };

    namespace detail_ {
        template <typename...>
        using dsVoid = void;

        template <typename, typename = void>
        struct dsIsValueHelper
        {
            static constexpr bool value = false;
        };

        template <typename T>
        struct dsIsValueHelper<T, dsVoid<decltype(dsValueTraits<T>::name[0])>>
        {
            // extra requirements based on current implementation of dsValueStorage
            static constexpr bool value = sizeof(T) <= 16 && alignof(T) <= alignof(void*);
        };

        template <>
        struct dsIsValueHelper<void, void>
        {
            static constexpr bool value = true;
        };

        consteval uint32_t dsHashTypeName(char const* name) noexcept
        {
            constexpr uint32_t prime = 0x0100'0193u;

            uint32_t hash = 0x811c'9dc5u;
            while (*name != '\0')
            {
                const uint8_t value = static_cast<uint8_t>(*name++);
                hash ^= value;
                hash *= prime;
            }
            return hash;
        }

        template <typename T>
        struct dsTypeMetaHolder
        {
            static constexpr dsTypeMeta meta{
                .name = dsValueTraits<T>::name,
                .typeId = dsHashTypeName(dsValueTraits<T>::name),
                .size = sizeof(T),
                .align = alignof(T),
                .opEquality = [](void const* left,
                                  void const* right) noexcept { return *static_cast<T const*>(left) == *static_cast<T const*>(right); },
                .opCopyTo = [](void* dest, void const* source) noexcept { new (dest) T(*static_cast<T const*>(source)); },
            };
        };

        template <>
        struct dsTypeMetaHolder<void>
        {
            static constexpr dsTypeMeta meta{
                .name = "void",
                .typeId = 0,
                .size = 0,
                .align = 0,
                .opEquality = [](void const*, void const*) noexcept { return true; },
                .opCopyTo = [](void*, void const*) noexcept {},
            };
        };
    }; // namespace detail_

    template <typename T>
    constexpr bool dsIsValue = detail_::dsIsValueHelper<T>::value;

    class dsTypeId final
    {
    public:
        constexpr dsTypeId() noexcept = default;
        constexpr explicit dsTypeId(dsTypeMeta const& meta) noexcept : meta_(&meta) {}

        constexpr explicit operator bool() const noexcept { return meta_->typeId != 0; }

        constexpr uint32_t id() const noexcept { return meta_->typeId; }
        constexpr dsTypeMeta const& meta() const noexcept { return *meta_; }

        constexpr bool operator==(dsTypeId rhs) const noexcept;
        constexpr bool operator==(dsTypeMeta const& rhs) const noexcept;
        constexpr std::strong_ordering operator<=>(dsTypeId rhs) const noexcept;

    private:
        dsTypeMeta const* meta_ = &detail_::dsTypeMetaHolder<void>::meta;
    };

    template <typename T>
    requires dsIsValue<T>
    constexpr dsTypeId dsType = detail_::dsTypeMetaHolder<T>::meta;

    template <typename T>
    requires dsIsValue<T>
    constexpr dsTypeId dsTypeOf(T const&) noexcept { return dsType<T>; }

    constexpr bool dsTypeId::operator==(dsTypeId rhs) const noexcept
    {
        if (meta_ == rhs.meta_)
            return true;
        return meta_->typeId == rhs.meta_->typeId;
    }

    constexpr bool dsTypeId::operator==(dsTypeMeta const& rhs) const noexcept
    {
        if (meta_ == &rhs)
            return true;
        return meta_->typeId == rhs.typeId;
    }

    constexpr std::strong_ordering dsTypeId::operator<=>(dsTypeId rhs) const noexcept
    {
        if (meta_ == rhs.meta_)
            return std::strong_ordering::equal;
        return meta_->typeId <=> rhs.meta_->typeId;
    }
} // namespace descript
