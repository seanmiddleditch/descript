// descript

#pragma once

#include "descript/alloc.hh"
#include "descript/export.hh"
#include "descript/types.hh"

namespace descript {
    class dsTypeDatabase
    {
    public:
        virtual void registerType(dsTypeId typeId) = 0;

        virtual dsTypeId lookupType(char const* name, char const* nameEnd = nullptr) const noexcept = 0;

    protected:
        ~dsTypeDatabase() = default;
    };

    DS_API [[nodiscard]] dsTypeDatabase* dsCreateTypeDatabase(dsAllocator& alloc);
    DS_API void dsDestroyTypeDatabase(dsTypeDatabase* database);
} // namespace descript
