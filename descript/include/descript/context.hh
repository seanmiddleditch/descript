// descript

#pragma once

#include "descript/types.hh"
#include "descript/value.hh"

namespace descript {
    class dsFunctionContext
    {
    public:
        virtual uint32_t argc() const noexcept = 0;
        virtual dsValue const& argAt(uint32_t index) const noexcept = 0;

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

        virtual bool readSlot(dsInputSlotIndex, dsValue& out_value) = 0;
        virtual bool readSlot(dsOutputSlotIndex, dsValue& out_value) = 0;

        virtual void writeSlot(dsOutputSlotIndex slotIndex, dsValue const& value) = 0;

        virtual void setPlugPower(dsOutputPlugIndex plugIndex, bool powered) = 0;

    protected:
        ~dsNodeContext() = default;
    };
} // namespace descript
