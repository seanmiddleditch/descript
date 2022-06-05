// descript

#pragma once

#include "assert.hh"

#include <cstring>

namespace descript {
    class dsString
    {
    public:
        explicit dsString(dsAllocator& alloc) noexcept : allocator_(&alloc) {}

        dsString(dsAllocator& alloc, char const* string, char const* sentinel = nullptr) : allocator_(&alloc) { reset(string, sentinel); }
        dsString(dsAllocator& alloc, decltype(nullptr), char const*) = delete;

        dsString(dsString&& rhs) noexcept : allocator_(rhs.allocator_), first_(rhs.first_), last_(rhs.last_)
        {
            rhs.first_ = rhs.last_ = emptyString;
        }
        dsString& operator=(dsString&& rhs) noexcept
        {
            allocator_ = rhs.allocator_;
            first_ = rhs.first_;
            last_ = rhs.last_;
            rhs.first_ = rhs.last_ = emptyString;
            return *this;
        }

        ~dsString() { reset(); }

        bool empty() const noexcept { return first_ == last_; }

        char const* cStr() const noexcept { return first_; }

        char const* data() const noexcept { return first_; }
        uint32_t size() const noexcept { return last_ - first_; }

        char const* begin() const noexcept { return first_; }
        char const* end() const noexcept { return last_; }

        inline void reset();
        inline void reset(char const* string, char const* sentinel = nullptr);
        inline void reset(decltype(nullptr), char const* sentinel = nullptr) = delete;

    private:
        // to avoid allocating for empty strings
        static constexpr char emptyString[] = "";

        dsAllocator* allocator_ = nullptr;
        char const* first_ = emptyString;
        char const* last_ = emptyString;
    };

    void dsString::reset()
    {
        if (*first_ != '\0')
            allocator_->free(const_cast<char*>(first_), last_ - first_ + 1 /*NUL*/, 1);
        first_ = last_ = emptyString;
    }

    void dsString::reset(char const* string, char const* sentinel)
    {
        if (string == nullptr)
            return reset();

        if (sentinel == nullptr)
            sentinel = string + std::strlen(string);

        uint32_t const size = static_cast<uint32_t>(sentinel - string);

        // allocate and copy before deallocating so that we can make
        // assign a sub-range of a string to itself
        //
        char* alloc = static_cast<char*>(allocator_->allocate(size + 1 /*NUL*/, 1));

        std::memcpy(alloc, string, size);
        alloc[size] = '\0';

        if (*first_ != '\0')
            allocator_->free(const_cast<char*>(first_), last_ - first_ + 1 /*NUL*/, 1);

        first_ = alloc;
        last_ = first_ + size;
    }
} // namespace descript
