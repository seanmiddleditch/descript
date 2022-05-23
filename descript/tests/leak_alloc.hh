// descript

#pragma once

#include "descript/alloc.hh"

#include "catch_amalgamated.hpp"

#include <exception>

namespace descript::test {
    class LeakTestAllocator final : public descript::dsAllocator
    {
    public:
        inline ~LeakTestAllocator();

        inline void* allocate(uint32_t size, uint32_t alignment) override;
        inline void free(void* block, uint32_t size, uint32_t alignment) override;

    private:
        descript::dsDefaultAllocator base_;
        uint32_t blocks_ = 0;
        uint32_t bytes_ = 0;
    };

    LeakTestAllocator::~LeakTestAllocator()
    {
        if (std::uncaught_exceptions() == 0)
        {
            CHECK(blocks_ == 0);
            CHECK(bytes_ == 0);
        }
    }

    void* LeakTestAllocator::allocate(uint32_t size, uint32_t alignment)
    {
        ++blocks_;
        bytes_ += size;
        return base_.allocate(size, alignment);
    }

    void LeakTestAllocator::free(void* block, uint32_t size, uint32_t alignment)
    {
        if (block != nullptr)
        {
            --blocks_;
            bytes_ -= bytes_;
            base_.free(block, size, alignment);
        }
    }
} // namespace descript::test
