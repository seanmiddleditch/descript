// descript

#pragma once

#include "descript/value.hh"

#include "assert.hh"

namespace descript {
    bool dsValueRef::operator==(dsValueRef const& right) const noexcept
    {
        if (type_ == right.type_)
            return type_.meta().opEquality != nullptr && type_.meta().opEquality(pointer_, right.pointer_);
        return false;
    }

    bool dsValueOut::accept(dsTypeId type, void const* pointer)
    {
        DS_GUARD_OR(sink_ != nullptr, false);

        if (sink_ != nullptr)
            return sink_(type, pointer, userData_);

        return false;
    }

} // namespace descript
