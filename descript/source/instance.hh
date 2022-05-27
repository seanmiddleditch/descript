// descript

#pragma once

#include "descript/types.hh"
#include "descript/value.hh"

#include "array.hh"
#include "event.hh"
#include "rel.hh"

#include <cstdint>

namespace descript {
    class dsAssembly;

    struct alignas(16) dsInstance final
    {
        explicit dsInstance(dsAllocator& alloc, dsInstanceId instanceId) noexcept : events(alloc), instanceId(instanceId) {}

        dsAssembly* assembly = nullptr;
        dsInstanceId instanceId;

        dsRelativeBitArray<dsAssemblyNodeIndex> activeNodes;
        dsRelativeBitArray<dsAssemblyOutputPlugIndex> activeInputPlugs;
        dsRelativeBitArray<dsAssemblyOutputPlugIndex> activeOutputPlugs;
        dsRelativeArray<dsValue, dsAssemblyVariableIndex> values;

        struct Event
        {
            dsAssemblyNodeIndex nodeIndex = dsInvalidIndex;
            dsEvent event;
        };

        dsArray<Event> events;
    };
} // namespace descript
