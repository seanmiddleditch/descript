// descript

#pragma once

#include "descript/export.hh"
#include "descript/types.hh"

namespace descript {
    class dsValueOut;

    class dsEvaluateHost
    {
    public:
        virtual void listen(dsEmitterId emitterId) = 0;

        virtual bool readConstant(uint32_t constantIndex, dsValueOut out_value) = 0;
        virtual bool readVariable(uint32_t variableIndex, dsValueOut out_value) = 0;
        virtual bool invokeFunction(uint32_t functionIndex, dsFunctionContext& ctx) = 0;

    protected:
        ~dsEvaluateHost() = default;
    };

    DS_API bool dsEvaluate(dsEvaluateHost& host, uint8_t const* ops, uint32_t opsLen, dsValueOut out_value);
} // namespace descript
