// descript

#pragma once

#include "descript/types.hh"

namespace descript {
    class dsValueOut;
    class dsValueRef;

    class dsFunctionContext
    {
    public:
        virtual uint32_t getArgCount() const noexcept = 0;
        virtual dsValueRef getArgValueAt(uint32_t index) const noexcept = 0;

        template <typename T>
        T getArgAt(uint32_t index)
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
        virtual dsInstanceId instanceId() const noexcept = 0;
        virtual dsNodeIndex nodeIndex() const noexcept = 0;

        virtual uint32_t numInputPlugs() const noexcept = 0;
        virtual uint32_t numOutputPlugs() const noexcept = 0;
        virtual uint32_t numInputSlots() const noexcept = 0;
        virtual uint32_t numOutputSlots() const noexcept = 0;

        virtual bool readSlot(dsInputSlotIndex, dsValueOut out_value) = 0;
        virtual bool readSlot(dsOutputSlotIndex, dsValueOut out_value) = 0;

        virtual void writeSlot(dsOutputSlotIndex slotIndex, dsValueRef const& value) = 0;

        virtual void setPlugPower(dsOutputPlugIndex plugIndex, bool powered) = 0;

    protected:
        ~dsNodeContext() = default;
    };
} // namespace descript
