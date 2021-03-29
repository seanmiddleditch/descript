// descript

#include <catch_amalgamated.hpp>

#include "descript/alloc.hh"
#include "descript/assembly.hh"
#include "descript/compiler.hh"

#include "array.hh"
#include "leak_alloc.hh"

using namespace descript;

namespace {
    constexpr dsNodeTypeId entryNodeTypeId{0xbaad};
    constexpr dsNodeTypeId stateNodeTypeId{0xf00d};
    constexpr dsNodeTypeId actionNodeTypeId{0xd00d};

    static constexpr dsNodeCompileMeta nodes[] = {
        {.typeId = entryNodeTypeId, .kind = dsNodeKind::Entry},
        {.typeId = stateNodeTypeId, .kind = dsNodeKind::State},
        {.typeId = actionNodeTypeId, .kind = dsNodeKind::Action},
    };

    class TestHost final : public dsCompilerHost
    {
    public:
        explicit TestHost(dsAllocator& alloc) noexcept : errors_(alloc) {}

        bool lookupNodeType(dsNodeTypeId typeId, dsNodeCompileMeta& out_nodeMeta) const noexcept override
        {
            for (dsNodeCompileMeta const& meta : nodes)
            {
                if (meta.typeId == typeId)
                {
                    out_nodeMeta = meta;
                    return true;
                }
            }
            return false;
        }

        bool lookupFunction(dsName name, dsFunctionCompileMeta& out_functionMeta) const noexcept override
        {
            return false;
        }

        void onError(dsCompileError const& error) override
        {
            INFO(static_cast<int>(error.code));
            errors_.pushBack(error);
        }

        uint32_t errorCount() const noexcept { return errors_.size(); }
        dsCompileErrorCode errorCode(uint32_t index) { return errors_[index].code; }

    private:
        dsArray<dsCompileError> errors_;
    };
} // namespace

TEST_CASE("Graph Compiler", "[compiler][graph]")
{
    test::LeakTestAllocator alloc;

    TestHost host(alloc);
    dsGraphCompiler* compiler = dsCreateGraphCompiler(alloc, host);

    SECTION("Just entry")
    {
        constexpr dsNodeId entryNode{0};

        compiler->beginNode(entryNode, entryNodeTypeId);
        compiler->addOutputPlug(dsDefaultOutputPlugIndex);

        CHECK(compiler->compile());
        CHECK(host.errorCount() == 0);
    }

    SECTION("Single simple state")
    {
        constexpr dsNodeId entryNode{0};
        constexpr dsNodeId stateNode{1};

        compiler->beginNode(entryNode, entryNodeTypeId);
        compiler->addOutputPlug(dsDefaultOutputPlugIndex);

        compiler->beginNode(stateNode, stateNodeTypeId);
        compiler->addInputPlug(dsBeginPlugIndex);

        compiler->addWire(entryNode, dsDefaultOutputPlugIndex, stateNode, dsBeginPlugIndex);

        CHECK(compiler->compile());
        CHECK(host.errorCount() == 0);
    }

    SECTION("Missing entry")
    {
        constexpr dsNodeId stateNode{0};

        compiler->beginNode(stateNode, stateNodeTypeId);
        compiler->addInputPlug(dsBeginPlugIndex);

        CHECK_FALSE(compiler->compile());
        CHECK(host.errorCount() == 1);

        CHECK(host.errorCode(0) == dsCompileErrorCode::NoEntries);
    }

    dsDestroyGraphCompiler(compiler);
}
