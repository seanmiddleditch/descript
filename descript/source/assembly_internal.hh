// descript

#pragma once

#include "descript/alloc.hh"
#include "descript/types.hh"

#include "index.hh"
#include "rel.hh"
#include "storage.hh"

#include <atomic>

namespace descript {
    DS_DEFINE_INDEX(dsAssemblyNodeIndex);
    DS_DEFINE_INDEX(dsAssemblyOutputPlugIndex);
    DS_DEFINE_INDEX(dsAssemblyWireIndex);
    DS_DEFINE_INDEX(dsAssemblyInputSlotIndex);
    DS_DEFINE_INDEX(dsAssemblyOutputSlotIndex);
    DS_DEFINE_INDEX(dsAssemblyVariableIndex);
    DS_DEFINE_INDEX(dsAssemblyDependencyIndex);
    DS_DEFINE_INDEX(dsAssemblyExpressionIndex);
    DS_DEFINE_INDEX(dsAssemblyConstantIndex);
    DS_DEFINE_INDEX(dsAssemblyFunctionIndex);
    DS_DEFINE_INDEX(dsAssemblyByteCodeIndex);

    struct dsAssemblyNode
    {
        dsNodeTypeId typeId;
        dsAssemblyOutputPlugIndex outputPlug;
        dsAssemblyOutputPlugIndex customOutputPlugStart;
        uint32_t customInputPlugCount = 0;
        uint32_t customOutputPlugCount = 0;
        dsAssemblyInputSlotIndex inputSlotStart;
        dsAssemblyOutputSlotIndex outputSlotStart;
        uint32_t inputSlotCount = 0;
        uint32_t outputSlotCount = 0;
    };

    struct dsAssemblyOutputPlug
    {
        dsAssemblyWireIndex wireStart;
        uint32_t wireCount = 0;
    };

    struct dsAssemblyWire
    {
        dsAssemblyNodeIndex nodeIndex;
        dsInputPlugIndex inputPlugIndex;
    };

    struct dsAssemblyInputSlot
    {
        // mutually exclusive; FIXME: condense space?
        dsAssemblyVariableIndex variableIndex;
        dsAssemblyExpressionIndex expressionIndex;
        dsAssemblyConstantIndex constantIndex;

        dsAssemblyNodeIndex nodeIndex;
    };

    struct dsAssemblyOutputSlot
    {
        dsAssemblyVariableIndex variableIndex;
    };

    struct dsAssemblyVariable
    {
        uint64_t nameHash = 0;
        dsAssemblyDependencyIndex dependencyStart;
        uint32_t dependencyCount = 0;
    };

    struct dsAssemblyDependency
    {
        dsAssemblyNodeIndex nodeIndex;
        dsAssemblyInputSlotIndex slotIndex;
    };

    struct dsAssemblyExpression
    {
        dsAssemblyByteCodeIndex codeStart;
        uint32_t codeCount = 0;
    };

    struct dsAssemblyConstant
    {
        uint32_t typeId = 0;
        uint64_t serialized = 0;
    };

    struct dsAssemblyHeader
    {
        uint32_t version = 0;
        uint32_t size = 0; // number of bytes, including header, payload, and all padding
        uint64_t hash = 0; // hash of header and all payload bytes, assuming padding and hash field are all 0

        uint32_t inputPlugCount = 0;
        dsRelativeArray<dsAssemblyNode, dsAssemblyNodeIndex> nodes;
        dsRelativeArray<dsAssemblyNodeIndex> entryNodes;
        dsRelativeArray<dsAssemblyOutputPlug, dsAssemblyOutputPlugIndex> outputPlugs;
        dsRelativeArray<dsAssemblyWire, dsAssemblyWireIndex> wires;
        dsRelativeArray<dsAssemblyInputSlot, dsAssemblyInputSlotIndex> inputSlots;
        dsRelativeArray<dsAssemblyOutputSlot, dsAssemblyOutputSlotIndex> outputSlots;
        dsRelativeArray<dsAssemblyVariable, dsAssemblyVariableIndex> variables;
        dsRelativeArray<dsAssemblyDependency, dsAssemblyDependencyIndex> dependencies;
        dsRelativeArray<dsAssemblyExpression, dsAssemblyExpressionIndex> expressions;
        dsRelativeArray<dsAssemblyConstant, dsAssemblyConstantIndex> constants;
        dsRelativeArray<dsFunctionId, dsAssemblyFunctionIndex> functions;
        dsRelativeArray<uint8_t, dsAssemblyByteCodeIndex> byteCode;
    };

    struct dsAssemblyNodeImpl
    {
        dsNodeFunction function = nullptr;
        uint32_t userOffset = 0;
    };

    struct dsAssemblyFunctionImpl
    {
        dsFunctionId functionId = dsInvalidFunctionId;
        dsFunction function = nullptr;
        void* userData = nullptr;
    };

    struct dsAssembly
    {
        dsAssembly(dsAllocator& alloc, uint32_t size) noexcept : allocator(alloc), assemblySize(size) {}

        std::atomic<uint32_t> references = 1;
        dsRelativeObject<dsAssemblyHeader> header;
        dsRelativeArray<dsAssemblyNodeImpl, dsAssemblyNodeIndex> nodes;
        dsRelativeArray<dsValueStorage, dsAssemblyConstantIndex> constants;
        dsRelativeArray<dsAssemblyFunctionImpl, dsAssemblyFunctionIndex> functions;
        uint32_t assemblySize = 0;
        uint32_t instanceSize = 0;
        uint32_t instanceStatesOffset = 0;
        uint32_t instanceInputPlugsOffset = 0;
        uint32_t instanceOutputPlugsOffset = 0;
        uint32_t instanceValuesOffset = 0;
        uint32_t instanceFunctionsOffset = 0;
        dsAllocator& allocator;
    };

    /// Validates that the provided range of bytes describes a valid assembly.
    bool dsValidateAssembly(uint8_t const* bytes, uint32_t size) noexcept;

    /// Calculates the hash of a assembly. Correctness requires all padding bytes to be 0.
    uint64_t dsHashAssembly(dsAssemblyHeader const* assembly) noexcept;
} // namespace descript
