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

            void registerType(dsTypeId typeId) override;

            dsTypeId lookupType(char const* name, char const* nameEnd = nullptr) const noexcept override;

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

    void TypeDatabase::registerType(dsTypeId typeId) { types_.pushBack(&typeId.meta()); }

    dsTypeId TypeDatabase::lookupType(char const* name, char const* nameEnd) const noexcept
    {
        DS_GUARD_OR(name != nullptr, dsTypeId());
        for (const dsTypeMeta* meta : types_)
            if (dsStrEqual(meta->name, name, nameEnd))
                return dsTypeId(*meta);
        return dsTypeId{};
    }
} // namespace descript
