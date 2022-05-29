// descript

#pragma once

#include "descript/key.hh"

#include <cstdint>

namespace descript {
    class dsFunctionContext;
    class dsNodeContext;
    class dsValueTypeId;
    class dsTypeId;

    // user-defined identifiers
    DS_DEFINE_KEY(dsNodeTypeId, uint64_t);
    DS_DEFINE_KEY(dsFunctionId, uint64_t);
    DS_DEFINE_KEY(dsNodeId, uint64_t);

    // user-defined identifiers, unique only within a single node
    DS_DEFINE_KEY(dsInputSlotIndex, uint8_t);
    DS_DEFINE_KEY(dsOutputSlotIndex, uint8_t);
    DS_DEFINE_KEY(dsInputPlugIndex, uint8_t);
    DS_DEFINE_KEY(dsOutputPlugIndex, uint8_t);

    // system-defined identifiers
    DS_DEFINE_KEY(dsEmitterId, uint64_t);
    DS_DEFINE_KEY(dsInstanceId, uint64_t);
    DS_DEFINE_KEY(dsNodeIndex, uint32_t);

    // invalid ids
    static constexpr dsEmitterId dsInvalidEmitterId{~uint64_t{0}};
    static constexpr dsNodeTypeId dsInvalidNodeTypeId{~uint64_t{0}};
    static constexpr dsInstanceId dsInvalidInstanceId{~uint64_t{0}};
    static constexpr dsFunctionId dsInvalidFunctionId{~uint64_t{0}};

    // special constants for plug indices
    static constexpr dsInputPlugIndex dsBeginPlugIndex{254};
    static constexpr dsOutputPlugIndex dsDefaultOutputPlugIndex{254};

    enum class dsNodeKind : uint8_t
    {
        Invalid,
        Entry,
        State,
        Action,
    };

    enum class dsPlugKind : uint8_t
    {
        Begin,
        Output,
        CustomInput,
        CustomOutput,
    };

    enum class dsEventType : uint8_t
    {
        Activate,
        Deactivate,
        Dependency,
        CustomInput,
    };

    struct dsName
    {
        char const* name = nullptr;
        char const* nameEnd = nullptr;
    };

    using dsFunction = void (*)(dsFunctionContext& context, void* userData);

    using dsNodeFunction = void (*)(dsNodeContext& context, dsEventType eventType, void* userData);

    struct dsParam final
    {
        dsName name;
        int32_t value = 0;
    };
} // namespace descript
