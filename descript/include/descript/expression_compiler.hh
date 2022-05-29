// descript

#pragma once

#include "descript/alloc.hh"
#include "descript/compile_types.hh"
#include "descript/export.hh"
#include "descript/types.hh"

namespace descript {
    class dsValue;

    class dsExpressionCompilerHost
    {
    public:
        virtual bool lookupVariable(dsName name, dsValueType& out_type) const noexcept = 0;
        virtual bool lookupFunction(dsName name, dsFunctionCompileMeta& out_functionMeta) const noexcept = 0;

    protected:
        ~dsExpressionCompilerHost() = default;
    };

    class dsExpressionBuilder
    {
    public:
        virtual void pushOp(uint8_t byte) = 0;

        virtual uint32_t pushConstant(dsValue const& value) = 0;
        virtual uint32_t pushFunction(dsFunctionId functionId) = 0;
        virtual uint32_t pushVariable(uint64_t nameHash) = 0;

    protected:
        ~dsExpressionBuilder() = default;
    };

    class dsExpressionCompiler
    {
    public:
        virtual void reset() = 0;

        virtual bool compile(char const* expression, char const* expressionEnd = nullptr) = 0;
        virtual bool optimize() = 0;
        virtual bool build(dsExpressionBuilder& builder) = 0;

        virtual bool isEmpty() const noexcept = 0;
        virtual bool isConstant() const noexcept = 0;
        virtual bool isVariableOnly() const noexcept = 0;
        virtual dsValueType resultType() const noexcept = 0;

        virtual bool asConstant(dsValue& out_value) const = 0;

    protected:
        ~dsExpressionCompiler() = default;
    };

    DS_API dsExpressionCompiler* dsCreateExpressionCompiler(dsAllocator& alloc, dsExpressionCompilerHost& host);
    DS_API void dsDestroyExpressionCompiler(dsExpressionCompiler* compiler);
}; // namespace descript
