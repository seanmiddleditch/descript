// descript

#pragma once

#include "descript/types.hh"

namespace descript {
    class dsValueOut;
    class dsValueRef;

    class dsFunctionContext
    {
    public:
        virtual uint32_t [[nodiscard]] getArgCount() const noexcept = 0;
        virtual dsValueRef [[nodiscard]] getArgValueAt(uint32_t index) const noexcept = 0;

        template <typename T>
        [[nodiscard]] T getArgAt(uint32_t index)
        {
            return getArgValueAt(index).template as<T>();
        }

        virtual void result(dsValueRef const& result) = 0;

        virtual void listen(dsEmitterId emitterId) = 0;

    protected:
        ~dsFunctionContext() = default;
    };

    class dsNodeContext
    {
    public:
        virtual [[nodiscard]] dsInstanceId instanceId() const noexcept = 0;
        virtual [[nodiscard]] dsNodeIndex nodeIndex() const noexcept = 0;

        virtual [[nodiscard]] uint32_t numInputPlugs() const noexcept = 0;
        virtual [[nodiscard]] uint32_t numOutputPlugs() const noexcept = 0;
        virtual [[nodiscard]] uint32_t numInputSlots() const noexcept = 0;
        virtual [[nodiscard]] uint32_t numOutputSlots() const noexcept = 0;

        virtual [[nodiscard]] bool readSlot(dsInputSlot slot, dsValueOut out_value) = 0;
        virtual [[nodiscard]] bool readOutputSlot(dsOutputSlot slot, dsValueOut out_value) = 0;

        virtual void writeSlot(dsOutputSlot slot, dsValueRef const& value) = 0;

        virtual void setPlugPower(dsOutputPlugIndex plugIndex, bool powered) = 0;

    protected:
        ~dsNodeContext() = default;
    };
} // namespace descript
