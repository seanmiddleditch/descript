// descript

#pragma once

#include "descript/export.hh"

#include <cstdint>

namespace descript {
    class dsAllocator;
    struct dsAssembly;
    class dsRuntimeHost;

    /// Constructs a runtime executable assembly
    DS_API [[nodiscard]] dsAssembly* dsLoadAssembly(dsAllocator& alloc, dsRuntimeHost& host, uint8_t const* bytes,
        uint32_t size);
    DS_API void dsAcquireAssembly(dsAssembly* assembly) noexcept;
    DS_API void dsReleaseAssembly(dsAssembly* assembly);
} // namespace descript
