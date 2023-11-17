// descript

#pragma once

#include "descript/value.hh"

#include "assert.hh"

namespace descript {
    bool dsValueRef::operator==(dsValueRef const& right) const noexcept
    {
        if (meta_->typeId == right.meta_->typeId)
            return meta_->opEquality != nullptr && meta_->opEquality(pointer_, right.pointer_);
        return false;
    }

    bool dsValueOut::accept(dsTypeMeta const& typeMeta, void const* pointer)
    {
        DS_GUARD_OR(sink_ != nullptr, false);

        if (sink_ != nullptr)
            return sink_(typeMeta, pointer, userData_);

        return false;
    }

} // namespace descript
