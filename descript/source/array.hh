// descript

#pragma once

#include "descript/alloc.hh"

#include "assert.hh"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace descript {
    template <typename Value, typename IndexT = uint32_t>
    class dsArray
    {
    public:
        static_assert(std::is_nothrow_destructible_v<Value>);
        static_assert(std::is_nothrow_move_constructible_v<Value>);

        using index_type = IndexT;

        ~dsArray() noexcept { deallocate(); }

        explicit dsArray(dsAllocator& allocator) noexcept : allocator_(&allocator) {}
        dsArray(dsArray&& rhs) noexcept : first_(rhs.first_), sentinel_(rhs.sentinel_), last_(rhs.last_), allocator_(rhs.allocator_)
        {
            rhs.first_ = rhs.sentinel_ = rhs.last_ = nullptr;
        }

        dsArray& operator=(dsArray&& rhs) noexcept
        {
            deallocate();
            first_ = rhs.first_;
            sentinel_ = rhs.sentinel_;
            last_ = rhs.last_;
            allocator_ = rhs.allocator_;
            rhs.first_ = rhs.sentinel_ = rhs.last_ = nullptr;
            return *this;
        }

        void resize(uint32_t size);
        void reserve(uint32_t minimumCapacity);

        uint32_t size() const noexcept { return sentinel_ - first_; }
        bool empty() const noexcept { return first_ == sentinel_; }

        Value* data() noexcept { return first_; }
        Value const* data() const noexcept { return first_; }

        Value* begin() noexcept { return first_; }
        Value const* begin() const noexcept { return first_; }

        Value* end() noexcept { return sentinel_; }
        Value const* end() const noexcept { return sentinel_; }

        Value& operator[](index_type index) noexcept
        {
            DS_ASSERT(static_cast<uint32_t>(index) < (sentinel_ - first_));
            return first_[static_cast<uint32_t>(index)];
        }
        Value const& operator[](index_type index) const noexcept
        {
            DS_ASSERT(static_cast<uint32_t>(index) < (sentinel_ - first_));
            return first_[static_cast<uint32_t>(index)];
        }

        Value& back() noexcept
        {
            DS_ASSERT(first_ != sentinel_);
            return *(sentinel_ - 1);
        }
        Value const& back() const noexcept
        {
            DS_ASSERT(first_ != sentinel_);
            return *(sentinel_ - 1);
        }

        bool contains(index_type index) const noexcept { return static_cast<uint32_t>(index) < (sentinel_ - first_); }

        void clear() noexcept;

        Value& pushBack(Value const& value);
        Value& pushBack(Value&& value);

        template <typename... Args>
        Value& emplaceBack(Args&&... args);

        Value popBack();

        dsAllocator& allocator() const noexcept { return *allocator_; }

    private:
        void reallocate(uint32_t required);
        void deallocate();

        using StorageValue = std::remove_const_t<Value>;

        StorageValue* first_ = nullptr;
        StorageValue* sentinel_ = nullptr;
        StorageValue* last_ = nullptr;
        dsAllocator* allocator_ = nullptr;
    };

    template <typename Value, typename IndexT>
    void dsArray<Value, IndexT>::resize(uint32_t size)
    {
        if (size < sentinel_ - first_)
        {
            Value* const newSentinel = first_ + size;
            if constexpr (!std::is_trivially_destructible_v<Value>)
            {
                for (StorageValue* item = newSentinel; item != sentinel_; ++item)
                    item->~Value();
            }
            sentinel_ = newSentinel;
        }
        else if (size > sentinel_ - first_)
        {
            reserve(size);
            Value* const newSentinel = first_ + size;
            for (StorageValue* item = sentinel_; item != newSentinel; ++item)
                new (item) StorageValue{};
            sentinel_ = newSentinel;
        }
    }

    template <typename Value, typename IndexT>
    void dsArray<Value, IndexT>::reserve(uint32_t minimumCapacity)
    {
        if (minimumCapacity > last_ - first_)
        {
            reallocate(minimumCapacity);
        }
    }

    template <typename Value, typename IndexT>
    void dsArray<Value, IndexT>::clear() noexcept
    {
        if (std::is_trivially_destructible_v<Value>)
        {
            sentinel_ = first_;
        }
        else
        {
            while (sentinel_ != first_)
                (--sentinel_)->~Value();
        }
    }

    template <typename Value, typename IndexT>
    auto dsArray<Value, IndexT>::pushBack(Value const& value) -> Value&
    {
        if (sentinel_ == last_)
        {
            uint32_t const cap = last_ - first_;
            uint32_t min = 16;
            if (cap < min)
            {
                reallocate(min);
            }
            else
            {
                uint32_t const req = cap + (cap >> 1); // grow by 50%
                reallocate(req);
            }
        }

        return *new (sentinel_++) Value(value);
    }

    template <typename Value, typename IndexT>
    auto dsArray<Value, IndexT>::pushBack(Value&& value) -> Value&
    {
        if (sentinel_ == last_)
        {
            uint32_t const cap = last_ - first_;
            reallocate(cap < 16 ? 16 : (cap + (cap >> 2)));
        }

        return *new (sentinel_++) Value(static_cast<Value&&>(value));
    }

    template <typename Value, typename IndexT>
    template <typename... Args>
    Value& dsArray<Value, IndexT>::emplaceBack(Args&&... args)
    {
        if (sentinel_ == last_)
        {
            uint32_t const cap = last_ - first_;
            reallocate(cap < 16 ? 16 : (cap + (cap >> 2)));
        }

        return *new (sentinel_++) Value(static_cast<Args&&>(args)...);
    }

    template <typename Value, typename IndexT>
    auto dsArray<Value, IndexT>::popBack() -> Value
    {
        DS_ASSERT(first_ != sentinel_);
        Value ret = std::move(*--sentinel_);
        sentinel_->~Value();
        return ret;
    }

    template <typename Value, typename IndexT>
    void dsArray<Value, IndexT>::reallocate(uint32_t required)
    {
        uint32_t const size = sentinel_ - first_;
        uint32_t const capacity = last_ - first_;

        if (capacity >= required)
            return;

        StorageValue* memory = static_cast<StorageValue*>(allocator_->allocate(required * sizeof(Value), alignof(Value)));
        if (first_ != nullptr)
        {
            if (std::is_trivially_move_constructible_v<Value>)
            {
                std::memcpy(memory, first_, size * sizeof(Value));
            }
            else
            {
                for (StorageValue *item = first_, *out = memory; item != sentinel_; ++item, ++out)
                {
                    new (out) StorageValue(static_cast<StorageValue&&>(*item));
                    item->~StorageValue();
                }
            }
        }

        deallocate();

        first_ = memory;
        sentinel_ = memory + size;
        last_ = memory + required;
    }

    template <typename Value, typename IndexT>
    void dsArray<Value, IndexT>::deallocate()
    {
        clear();
        allocator_->free(const_cast<std::remove_const_t<Value>*>(first_), (last_ - first_) * sizeof(Value), alignof(Value));
        first_ = sentinel_ = last_ = nullptr;
    }
} // namespace descript
