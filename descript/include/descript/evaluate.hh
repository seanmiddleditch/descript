// descript

#pragma once

#include "descript/export.hh"
#include "descript/types.hh"

namespace descript {
    class dsValue;

    class dsEvaluateHost
    {
    public:
        virtual void listen(dsEmitterId emitterId) = 0;

        virtual bool readConstant(uint32_t constantIndex, dsValue& out_value) = 0;
        virtual bool readVariable(uint32_t variableIndex, dsValue& out_value) = 0;
        virtual bool invokeFunction(uint32_t functionIndex, dsFunctionContext& ctx, dsValue& out_result) = 0;

    protected:
        ~dsEvaluateHost() = default;
    };

    DS_API bool dsEvaluate(dsEvaluateHost& host, uint8_t const* ops, uint32_t opsLen, dsValue& out_result);
} // namespace descript
