// descript

#pragma once

#include "descript/compile_types.hh"
#include "descript/export.hh"
#include "descript/types.hh"

#include <cstdint>

namespace descript {
    class dsAllocator;
    class dsGraphCompiler;
    class dsValueRef;

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

        // set metadata about the current graph
        virtual void setGraphName(char const* name, char const* nameEnd = nullptr) = 0;
        virtual void setDebugName(char const* name, char const* nameEnd = nullptr) = 0;

        // add a variable declaration
        virtual void addVariable(dsTypeId type, char const* name, char const* nameEnd = nullptr) = 0;

        // begin a node
        virtual void beginNode(dsNodeId nodeId, dsNodeTypeId nodeTypeId) = 0;

        // begin a slot, bound to current node
        virtual void beginInputSlot(dsInputSlot slot, dsTypeId type) = 0;
        virtual void beginOutputSlot(dsOutputSlot slot, dsTypeId type) = 0;

        // bind values to current slot; output slots only support bindVariable
        virtual void bindVariable(char const* name, char const* nameEnd = nullptr) = 0;
        virtual void bindExpression(char const* expression, char const* expressionEnd = nullptr) = 0;
        virtual void bindConstant(dsValueRef const& value) = 0;

        // add new plugs, bound to current node
        virtual void addInputPlug(dsInputPlugIndex inputPlugIndex) = 0;
        virtual void addOutputPlug(dsOutputPlugIndex outputPlugIndex) = 0;

        // add a wire between two plugs
        virtual void addWire(dsNodeId fromNodeId, dsOutputPlugIndex fromPlugIndex, dsNodeId toNodeId, dsInputPlugIndex toPlugIndex) = 0;

        // compiles defined graph, validates for errors and builds internal state
        virtual [[nodiscard]] bool compile() = 0;

        // creates an assembly for serialization; only allowed after compile() returns true
        virtual [[nodiscard]] bool build() = 0;

        // queries the errors that have occured for the current graph
        virtual [[nodiscard]] uint32_t getErrorCount() const noexcept = 0;
        virtual [[nodiscard]] dsCompileError getError(uint32_t index) const noexcept = 0;

        // retrives the serialized assembly, only valid after build() returns true
        virtual [[nodiscard]] uint8_t const* assemblyBytes() const noexcept = 0;
        virtual [[nodiscard]] uint32_t assemblySize() const noexcept = 0;

    protected:
        ~dsGraphCompiler() = default;
    };

    DS_API [[nodiscard]] dsGraphCompiler* dsCreateGraphCompiler(dsAllocator& alloc, dsGraphCompilerHost& host);
    DS_API void dsDestroyGraphCompiler(dsGraphCompiler* compiler);

} // namespace descript
