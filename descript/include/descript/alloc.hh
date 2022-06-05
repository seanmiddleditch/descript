// descript

#pragma once

#include "descript/export.hh"

#include <cstdint>

namespace descript {
    class dsAllocator
    {
    public:
        virtual [[nodiscard]] void* allocate(uint32_t size, uint32_t alignment) = 0;
        virtual void free(void* block, uint32_t size, uint32_t alignment) = 0;

    protected:
        ~dsAllocator() = default;
    };

    class DS_API dsDefaultAllocator final : public dsAllocator
    {
    public:
        [[nodiscard]] void* allocate(uint32_t size, uint32_t alignment) override;
        void free(void* block, uint32_t size, uint32_t alignment) override;
    };
} // namespace descript
