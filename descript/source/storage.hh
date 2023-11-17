// descript

#pragma once

#include "descript/meta.hh"
#include "descript/value.hh"

namespace descript {
    class dsValueStorage final
    {
    public:
        constexpr dsValueStorage() noexcept = default;

        dsValueStorage(dsValueRef const& value) noexcept
        {
            meta_ = &value.meta();
            meta_->opCopyTo(&storage_, value.pointer());
        }

        template <typename T>
        requires dsIsValue<T>
        /*implicit*/ dsValueStorage(T const& value)
        noexcept : meta_(&dsType<T>) { new (&storage_) T(value); }

        constexpr [[nodiscard]] dsTypeId type() const noexcept { return dsTypeId{meta_->typeId}; }

        [[nodiscard]] void const* pointer() const noexcept { return &storage_; }

        template <typename T>
        requires dsIsValue<T>
        constexpr [[nodiscard]] bool is() const noexcept { return meta_->typeId == dsType<T>.typeId; }

        template <typename T>
        requires dsIsValue<T>
        constexpr T as() const noexcept { return *static_cast<T const*>(static_cast<void const*>(&storage_)); }

        [[nodiscard]] dsValueRef ref() const noexcept { return dsValueRef(*meta_, &storage_); }

        [[nodiscard]] dsValueOut out() noexcept { return dsValueOut(&sink, this); }

        constexpr [[nodiscard]] bool operator==(dsValueStorage const& right) const noexcept
        {
            return meta_->typeId == right.meta_->typeId && meta_->opEquality != nullptr && meta_->opEquality(&storage_, &right.storage_);
        }

    private:
        static bool sink(dsTypeMeta const& typeMeta, void const* pointer, void* userData)
        {
            dsValueStorage& self = *static_cast<dsValueStorage*>(userData);
            self.meta_ = &typeMeta;
            self.meta_->opCopyTo(&self.storage_, pointer);
            return true;
        }

        dsTypeMeta const* meta_ = &dsType<void>;
        alignas(void*) char storage_[16];

        friend class dsValueRef;
        friend class dsValueOut;
    };
} // namespace descript
