// descript

#pragma once

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

    struct dsFunctionSignature
    {
        dsValueType const* paramTypes = nullptr;
        dsFunctionSignature* next = nullptr;
        dsValueType returnType = dsValueType::Nil;
        uint32_t paramCount = 0;
    };

    struct dsFunctionCompileMeta
    {
        char const* name = nullptr;
        dsFunctionId functionId = dsInvalidFunctionId;
        dsValueType returnType = dsValueType::Nil;
    };
}
