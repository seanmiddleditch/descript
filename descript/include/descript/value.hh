// descript

#pragma once

#include <cstdint>

#include "descript/types.hh"

namespace descript {
    class dsValue final
    {
    public:
        constexpr dsValue() noexcept = default;
        constexpr /*implicit*/ dsValue(double val) noexcept : data_{.f64 = val}, type_{dsValueType::Double} {}
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
            double f64 = 0.0;
            bool b;
        } data_;
        dsValueType type_ = dsValueType::Nil;

        template <typename T>
        struct CastHelper;
    };

    template <>
    struct dsValue::CastHelper<double>
    {
        static constexpr bool is(dsValue const& val) noexcept { return val.type_ == dsValueType::Double; }
        static constexpr double as(dsValue const& val) noexcept { return val.data_.f64; }
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
        case dsValueType::Double: return data_.f64 == right.data_.f64;
        case dsValueType::Bool: return data_.b == right.data_.b;
        default: return false; // unknown type
        }
    }
} // namespace descript
