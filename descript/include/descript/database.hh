// descript

#pragma once

#include "descript/alloc.hh"
#include "descript/export.hh"
#include "descript/types.hh"

namespace descript {
    class dsTypeMeta;

    class dsTypeDatabase
    {
    public:
        virtual void registerType(dsTypeMeta const& meta) = 0;

        virtual dsTypeId lookupType(char const* name, char const* nameEnd = nullptr) const noexcept = 0;
        virtual dsTypeMeta const* getMeta(dsTypeId typeId) const noexcept = 0;

    protected:
        ~dsTypeDatabase() = default;
    };

    DS_API [[nodiscard]] dsTypeDatabase* dsCreateTypeDatabase(dsAllocator& alloc);
    DS_API void dsDestroyTypeDatabase(dsTypeDatabase* database);
} // namespace descript
