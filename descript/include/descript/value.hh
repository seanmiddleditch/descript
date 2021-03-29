// descript

#pragma once

#include <cstdint>

namespace descript {

    enum class dsValueType : uint8_t
    {
        Double,
    };

    class dsValue final
    {
    public:
        constexpr dsValue() noexcept = default;
        constexpr explicit dsValue(double val) noexcept : data_{.f64 = val}, type_{dsValueType::Double} {}

        constexpr dsValueType type() const noexcept { return type_; }

        template <typename T>
        constexpr bool is() const noexcept;

        template <typename T>
        constexpr T as() const noexcept;

        constexpr bool operator==(dsValue const& right) const noexcept;

    private:
        union {
            double f64 = 0.0;
        } data_;
        dsValueType type_ = dsValueType::Double;

        template <typename T>
        struct CastHelper;
    };

    template <>
    struct dsValue::CastHelper<double>
    {
        static constexpr bool is(dsValue const& val) noexcept { return val.type_ == dsValueType::Double; }
        static constexpr double as(dsValue const& val) noexcept { return val.data_.f64; }
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

        return data_.f64 == right.data_.f64;
    }
} // namespace descript
