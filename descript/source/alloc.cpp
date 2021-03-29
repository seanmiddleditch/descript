// descript

#include "descript/alloc.hh"

#include <new>

namespace descript {
    void* dsDefaultAllocator::allocate(uint32_t size, uint32_t alignment) { return ::operator new(size, std::align_val_t(alignment)); }

    void dsDefaultAllocator::free(void* block, uint32_t size, uint32_t alignment)
    {
        ::operator delete(block, size, std::align_val_t(alignment));
    }
} // namespace descript
