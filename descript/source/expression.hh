// descript

#pragma once

#include "descript/export.hh"
#include "descript/types.hh"

#include "index.hh"

#include <cstdint>

namespace descript {
    class dsValue;

    DS_DEFINE_INDEX(dsExpressionConstantIndex);
    DS_DEFINE_INDEX(dsExpressionVariableIndex);
    DS_DEFINE_INDEX(dsExpressionFunctionIndex);

    class dsFunctionContext
    {
    public:
        virtual uint32_t argc() const noexcept = 0;
        virtual dsValue const& argAt(uint32_t index) const noexcept = 0;

        virtual void listen(dsEmitterId emitterId) = 0;

    protected:
        ~dsFunctionContext() = default;
    };

    class dsEvaluateHost
    {
    public:
        virtual void listen(dsEmitterId emitterId) = 0;

        virtual bool readConstant(dsExpressionConstantIndex constantIndex, dsValue& out_value) = 0;
        virtual bool readVariable(dsExpressionVariableIndex variableIndex, dsValue& out_value) = 0;
        virtual bool invokeFunction(dsExpressionFunctionIndex functionIndex, dsFunctionContext& ctx, dsValue& out_result) = 0;

    protected:
        ~dsEvaluateHost() = default;
    };

    DS_API bool dsEvaluate(dsEvaluateHost& host, uint8_t const* ops, uint32_t opsLen, dsValue& out_result);
} // namespace descript
