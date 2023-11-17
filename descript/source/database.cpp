// descript

#include "descript/database.hh"
#include "descript/meta.hh"

#include "array.hh"
#include "utility.hh"

namespace descript {
    namespace {
        class TypeDatabase final : public dsTypeDatabase
        {
        public:
            explicit TypeDatabase(dsAllocator& alloc) noexcept : allocator_(alloc), types_(alloc) {}

            void registerType(dsTypeMeta const& meta) override;

            dsTypeId lookupType(char const* name, char const* nameEnd = nullptr) const noexcept override;
            dsTypeMeta const* getMeta(dsTypeId typeId) const noexcept override;

            dsAllocator& allocator() noexcept { return allocator_; }

        private:
            dsAllocator& allocator_;
            dsArray<dsTypeMeta const*> types_;
        };
    } // namespace

    dsTypeDatabase* dsCreateTypeDatabase(dsAllocator& alloc)
    {
        TypeDatabase* database = new (alloc.allocate(sizeof(TypeDatabase), alignof(TypeDatabase))) TypeDatabase(alloc);
        database->registerType(dsType<void>);
        database->registerType(dsType<int32_t>);
        database->registerType(dsType<float>);
        database->registerType(dsType<bool>);
        return database;
    }

    void dsDestroyTypeDatabase(dsTypeDatabase* database)
    {
        if (database != nullptr)
        {
            TypeDatabase& impl = *static_cast<TypeDatabase*>(database);
            dsAllocator& alloc = impl.allocator();
            impl.~TypeDatabase();
            alloc.free(&impl, sizeof(TypeDatabase), alignof(TypeDatabase));
        }
    }

    void TypeDatabase::registerType(dsTypeMeta const& meta) { types_.pushBack(&meta); }

    dsTypeId TypeDatabase::lookupType(char const* name, char const* nameEnd) const noexcept
    {
        DS_GUARD_OR(name != nullptr, dsTypeId{dsType<void>.typeId});

        for (const dsTypeMeta* meta : types_)
            if (dsStrEqual(meta->name, name, nameEnd))
                return dsTypeId(meta->typeId);

        return dsTypeId{dsType<void>.typeId};
    }

    dsTypeMeta const* TypeDatabase::getMeta(dsTypeId typeId) const noexcept
    {
        for (const dsTypeMeta* meta : types_)
            if (meta->typeId == typeId)
                return meta;
        return nullptr;
    }
} // namespace descript
