// descript

#pragma once

#include "descript/export.hh"
#include "descript/types.hh"

namespace descript {
    class dsAllocator;
    class dsInstance;
    struct dsAssembly;
    class dsChangeBus;

    struct dsNodeRuntimeMeta final
    {
        dsNodeTypeId typeId = dsInvalidNodeTypeId;
        dsNodeFunction function = nullptr;
        uint32_t userSize = 0;
        uint32_t userAlign = alignof(void*);
    };

    struct dsFunctionRuntimeMeta final
    {
        dsFunctionId functionId = dsInvalidFunctionId;
        dsFunction function = nullptr;
        void* userData = nullptr;
    };

    class dsRuntimeHost
    {
    public:
        virtual bool lookupNode(dsNodeTypeId, dsNodeRuntimeMeta& out_meta) const noexcept = 0;
        virtual bool lookupFunction(dsFunctionId, dsFunctionRuntimeMeta& out_meta) const noexcept = 0;

    protected:
        ~dsRuntimeHost() = default;
    };

    class dsRuntime
    {
    public:
        virtual dsInstanceId createInstance(dsAssembly* assembly, dsParam const* params = nullptr, uint32_t paramCount = 0) = 0;
        virtual void destroyInstance(dsInstanceId instanceId) = 0;

        virtual bool writeVariable(dsInstanceId instanceId, dsName variable, dsValue const& value) = 0;
        virtual bool readVariable(dsInstanceId instanceId, dsName variable, dsValue& out_value) = 0;

        virtual void processEvents() = 0;

        virtual dsEmitterId makeEmitterId() = 0;
        virtual void notifyChange(dsEmitterId emitterId) = 0;

    protected:
        ~dsRuntime() = default;
    };

    DS_API dsRuntime* dsCreateRuntime(dsAllocator& alloc, dsRuntimeHost& host);
    DS_API void dsDestroyRuntime(dsRuntime* runtime);
} // namespace descript
