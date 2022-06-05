// descript

#pragma once

#include <compare>
#include <cstdint>

namespace descript {
    static constexpr struct
    {
        constexpr explicit operator uint32_t() const noexcept { return ~uint32_t{0u}; }
    } dsInvalidIndex;

    template <typename DerivedT>
    class dsIndex
    {
    public:
        using underlying_type = uint32_t;

        constexpr explicit dsIndex(underlying_type value) noexcept : value_(value) {}
        constexpr dsIndex(decltype(dsInvalidIndex)) noexcept : value_(invalid_value) {}

        constexpr underlying_type value() const noexcept { return value_; }
        constexpr explicit operator underlying_type() const noexcept { return value_; }

        constexpr std::strong_ordering operator<=>(dsIndex const&) const noexcept = default;
        constexpr bool operator==(dsIndex const&) const noexcept = default;
        constexpr bool operator==(underlying_type rhs) const noexcept { return value_ == rhs; }

        constexpr DerivedT operator+(underlying_type rhs) const noexcept { return DerivedT{value_ + rhs}; }
        constexpr DerivedT& operator++() noexcept
        {
            ++value_;
            return static_cast<DerivedT&>(*this);
        }

    private:
        static constexpr underlying_type invalid_value = ~underlying_type{0};

        underlying_type value_ = invalid_value;
    };
} // namespace descript

// clang-format off
#define DS_DEFINE_INDEX(name) \
    class name final : public dsIndex<class name> { public: using dsIndex::dsIndex; }
// clang-format on
