// descript

#pragma once

#include "descript/compile_types.hh"
#include "descript/export.hh"
#include "descript/types.hh"

#include <cstdint>

namespace descript {
    class dsAllocator;
    class dsGraphCompiler;
    class dsValue;

    class dsGraphCompilerHost
    {
    public:
        virtual bool lookupNodeType(dsNodeTypeId typeId, dsNodeCompileMeta& out_nodeMeta) const noexcept = 0;
        virtual bool lookupFunction(dsName name, dsFunctionCompileMeta& out_functionMeta) const noexcept = 0;

    protected:
        ~dsGraphCompilerHost() = default;
    };

    class dsGraphCompiler
    {
    public:
        virtual void reset() = 0;

        virtual void setGraphName(char const* name, char const* nameEnd = nullptr) = 0;
        virtual void setDebugName(char const* name, char const* nameEnd = nullptr) = 0;

        virtual void beginNode(dsNodeId nodeId, dsNodeTypeId nodeTypeId) = 0;

        virtual void addInputSlot(dsInputSlotIndex slotIndex) = 0;
        virtual void addOutputSlot(dsOutputSlotIndex slotIndex) = 0;

        virtual void addInputPlug(dsInputPlugIndex inputPlugIndex) = 0;
        virtual void addOutputPlug(dsOutputPlugIndex outputPlugIndex) = 0;

        virtual void addWire(dsNodeId fromNodeId, dsOutputPlugIndex fromPlugIndex, dsNodeId toNodeId, dsInputPlugIndex toPlugIndex) = 0;

        virtual void addVariable(dsValueType type, char const* name, char const* nameEnd = nullptr) = 0;

        virtual void bindSlotVariable(dsNodeId nodeId, dsInputSlotIndex slotIndex, char const* name, char const* nameEnd = nullptr) = 0;
        virtual void bindSlotExpression(dsNodeId nodeId, dsInputSlotIndex slotIndex, char const* expression,
            char const* expressionEnd = nullptr) = 0;
        virtual void bindOutputSlotVariable(dsNodeId nodeId, dsOutputSlotIndex slotIndex, char const* name,
            char const* nameEnd = nullptr) = 0;

        virtual bool compile() = 0;
        virtual bool build() = 0;

        virtual uint32_t getErrorCount() const noexcept = 0;
        virtual dsCompileError getError(uint32_t index) const noexcept = 0;

        virtual uint8_t const* assemblyBytes() const noexcept = 0;
        virtual uint32_t assemblySize() const noexcept = 0;

    protected:
        ~dsGraphCompiler() = default;
    };

    DS_API dsGraphCompiler* dsCreateGraphCompiler(dsAllocator& alloc, dsGraphCompilerHost& host);
    DS_API void dsDestroyGraphCompiler(dsGraphCompiler* compiler);

} // namespace descript
