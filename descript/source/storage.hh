// descript

#pragma once

#include "descript/meta.hh"
#include "descript/value.hh"

namespace descript {
    class dsValueStorage final
    {
    public:
        constexpr dsValueStorage() noexcept = default;

        dsValueStorage(dsValueRef const& value) noexcept {
            type_ = value.type();
            type_.meta().opCopyTo(&storage_, value.pointer());
        }

        template <typename T>
        requires dsIsValue<T>
        /*implicit*/ dsValueStorage(T const& value)
        noexcept : type_(dsType<T>) { new (&storage_) T(value); }

        constexpr explicit operator bool() const noexcept { return (bool)type_; }

        constexpr dsTypeId type() const noexcept { return type_; }

        void const* pointer() const noexcept { return &storage_; }

        template <typename T>
        requires dsIsValue<T>
        constexpr bool is() const noexcept { return type_ == dsType<T>; }

        template <typename T>
        requires dsIsValue<T>
        constexpr T as() const noexcept { return *static_cast<T const*>(static_cast<void const*>(&storage_)); }

        dsValueRef ref() const noexcept {
            return dsValueRef(type_, &storage_);
        }

        dsValueOut out() noexcept {
            return dsValueOut(&sink, this);
        }

        constexpr bool operator==(dsValueStorage const& right) const noexcept
        {
            return type_ == right.type_ && type_.meta().opEquality != nullptr && type_.meta().opEquality(&storage_, &right.storage_);
        }

    private:
        static bool sink(dsTypeId type, void const* pointer, void* userData)
        {
            dsValueStorage& self = *static_cast<dsValueStorage*>(userData);
            self.type_ = type;
            type.meta().opCopyTo(&self.storage_, pointer);
            return true;
        }

        dsTypeId type_;
        alignas(void*) char storage_[16];

        friend class dsValueRef;
        friend class dsValueOut;
    };
} // namespace descript
