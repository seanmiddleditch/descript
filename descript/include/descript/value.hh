// descript

#pragma once

#include "descript/export.hh"
#include "descript/meta.hh"
#include "descript/types.hh"

namespace descript {
    using dsValueSink = bool (*)(dsTypeId type, void const* pointer, void* userData);

    class dsValueRef final
    {
    public:
        explicit dsValueRef(dsTypeId type, void const* pointer) noexcept : type_(type), pointer_(pointer) {}

        template <typename T>
        requires dsIsValue<T>
        /*implicit*/ dsValueRef(T const& value)
        noexcept : type_(dsType<T>), pointer_(&value) {}

        dsValueRef(dsValueRef const&) = delete;

        [[nodiscard]] dsTypeId type() const noexcept { return type_; }
        [[nodiscard]] void const* pointer() const noexcept { return pointer_; }

        [[nodiscard]] bool is(dsTypeId type) const noexcept { return type_ == type; }

        template <typename T>
        requires dsIsValue<T>
        [[nodiscard]] bool is() const noexcept { return is(dsTypeOf<T>()); }

        template <typename T>
        requires dsIsValue<T>
        [[nodiscard]] T as() const noexcept { return *static_cast<T const*>(static_cast<void const*>(pointer_)); }

        dsValueRef& operator=(dsValueRef const&) = delete;

        DS_API [[nodiscard]] bool operator==(dsValueRef const& right) const noexcept;

    private:
        dsTypeId type_;
        void const* pointer_ = nullptr;
    };

    class dsValueOut final
    {
    public:
        explicit dsValueOut(dsValueSink sink, void* userData) noexcept : sink_(sink), userData_(userData) {}

        template <typename T>
        dsValueOut& operator=(T const&) = delete;

        [[nodiscard]] bool accept(dsValueRef const& value) { return accept(value.type(), value.pointer()); }

        template <typename T>
        requires dsIsValue<T>
        [[nodiscard]] bool accept(T const& value) { return accept(dsType<T>, &value); }

        DS_API [[nodiscard]] bool accept(dsTypeId type, void const* pointer);

    private:
        dsValueSink sink_ = nullptr;
        void* userData_ = nullptr;
    };
} // namespace descript
