// descript

#pragma once

#include <cstdint>

#include "descript/types.hh"

namespace descript {
    class dsValue final
    {
    public:
        constexpr dsValue() noexcept = default;
        constexpr /*implicit*/ dsValue(float val) noexcept : data_{.f32 = val}, type_{dsValueType::Float32} {}
        constexpr /*implicit*/ dsValue(int32_t val) noexcept : data_{.i32 = val}, type_{dsValueType::Int32} {}
        constexpr /*implicit*/ dsValue(bool val) noexcept : data_{.b = val}, type_{dsValueType::Bool} {}
        constexpr /*implicit*/ dsValue(decltype(nullptr)) noexcept : type_{dsValueType::Nil} {}
        template <typename T>
        dsValue(T const&) = delete;

        constexpr dsValueType type() const noexcept { return type_; }

        template <typename T>
        constexpr bool is() const noexcept;

        template <typename T>
        constexpr T as() const noexcept;

        constexpr bool operator==(dsValue const& right) const noexcept;

    private:
        union {
            float f32 = 0.0;
            int32_t i32;
            bool b;
        } data_;
        dsValueType type_ = dsValueType::Nil;

        template <typename T>
        struct CastHelper;
    };

    template <>
    struct dsValue::CastHelper<float>
    {
        static constexpr bool is(dsValue const& val) noexcept { return val.type_ == dsValueType::Float32; }
        static constexpr double as(dsValue const& val) noexcept { return val.data_.f32; }
    };

        template <>
    struct dsValue::CastHelper<int32_t>
    {
        static constexpr bool is(dsValue const& val) noexcept { return val.type_ == dsValueType::Int32; }
        static constexpr int32_t as(dsValue const& val) noexcept { return val.data_.i32; }
    };

    template <>
    struct dsValue::CastHelper<bool>
    {
        static constexpr bool is(dsValue const& val) noexcept { return val.type_ == dsValueType::Bool; }
        static constexpr bool as(dsValue const& val) noexcept { return val.data_.b; }
    };

    template <typename T>
    constexpr bool dsValue::is() const noexcept
    {
        return dsValue::CastHelper<T>::is(*this);
    }

    template <typename T>
    constexpr T dsValue::as() const noexcept
    {
        return dsValue::CastHelper<T>::as(*this);
    }

    constexpr bool dsValue::operator==(dsValue const& right) const noexcept
    {
        if (type_ != right.type_)
            return false;

        switch (type_)
        {
        case dsValueType::Nil: return true; // nil == nil
        case dsValueType::Int32: return data_.i32 == right.data_.i32;
        case dsValueType::Float32: return data_.f32 == right.data_.f32;
        case dsValueType::Bool: return data_.b == right.data_.b;
        default: return false; // unknown type
        }
    }
} // namespace descript
