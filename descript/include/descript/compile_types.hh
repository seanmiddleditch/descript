// descript

#pragma once

#include "descript/meta.hh"
#include "descript/types.hh"

namespace descript {
    enum class dsCompileErrorCode
    {
        Unknown,
        NoEntries,
        DuplicateBuiltinPlug,
        DuplicateSlotBinding,
        UnknownNodeType,
        IllegalPlugPower,
        IllegalPlugCustomId,
        IncompatiblePowerWire,
        NodeNotFound,
        PlugNotFound,
        SlotNotFound,
        VariableNotFound,
        ExpressionCompileError,
        IncompatibleType,
    };

    struct dsCompileError final
    {
        dsCompileErrorCode code = dsCompileErrorCode::Unknown;
    };

    struct dsNodeCompileMeta
    {
        dsNodeTypeId typeId = dsInvalidNodeTypeId;
        dsNodeKind kind = dsNodeKind::State;
    };

    struct dsVariableCompileMeta
    {
        dsTypeId type;
    };

    struct dsFunctionSignature
    {
        dsTypeId const* paramTypes = nullptr;
        dsFunctionSignature* next = nullptr;
        dsTypeId returnType;
        uint32_t paramCount = 0;
    };

    struct dsFunctionCompileMeta
    {
        char const* name = nullptr;
        dsFunctionId functionId = dsInvalidFunctionId;
        dsTypeId returnType;
    };
}
