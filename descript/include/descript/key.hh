// descript

#pragma once

#include <compare>
#include <cstdint>

namespace descript {
    template <typename TagT, typename UnderlyingT>
    class dsKey
    {
    public:
        constexpr explicit dsKey(UnderlyingT value) noexcept : value_(value) {}

        constexpr UnderlyingT value() const noexcept { return value_; }

        constexpr std::strong_ordering operator<=>(dsKey const&) const noexcept = default;
        constexpr bool operator==(dsKey const&) const noexcept = default;

    private:
        UnderlyingT value_;
    };

    // clang-format off
#define DS_DEFINE_KEY(name, base) \
    class name final : public dsKey<class name, base> { public: using dsKey::dsKey; };
    // clang-format on

} // namespace descript
