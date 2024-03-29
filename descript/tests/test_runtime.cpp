// descript

#include <catch_amalgamated.hpp>

#include "descript/alloc.hh"
#include "descript/assembly.hh"
#include "descript/context.hh"
#include "descript/database.hh"
#include "descript/evaluate.hh"
#include "descript/graph_compiler.hh"
#include "descript/node.hh"
#include "descript/runtime.hh"
#include "descript/value.hh"

#include "array.hh"
#include "fnv.hh"
#include "leak_alloc.hh"
#include "storage.hh"
#include "utility.hh"

using namespace descript;

namespace {
    static bool canaryValue = false;
    static bool flagValue = false;
    static dsEmitterId flagEmitterId = dsInvalidEmitterId;

    class EmptyState final : public NodeVirtualBase<EmptyState>
    {
    public:
        static constexpr dsNodeTypeId typeId{dsHashFnv1a64("EmptyState")};
        static constexpr dsNodeKind kind{dsNodeKind::State};

        void onActivate(dsNodeContext& ctx) override {}
    };

    class ConditionState final : public NodeVirtualBase<ConditionState>
    {
    public:
        static constexpr dsNodeTypeId typeId{dsHashFnv1a64("ConditionState")};
        static constexpr dsNodeKind kind{dsNodeKind::State};

        static constexpr dsOutputPlugIndex truePlug{0};
        static constexpr dsOutputPlugIndex falsePlug{1};

        static constexpr dsInputSlot conditionSlot = dsInputSlot(0);

        void onActivate(dsNodeContext& ctx) override { Update(ctx); }
        void onDependency(dsNodeContext& ctx) override { Update(ctx); }

    private:
        void Update(dsNodeContext& ctx)
        {
            dsValueStorage value;
            if (!ctx.readSlot(conditionSlot, value.out()))
                value = dsValueStorage{false};
            ctx.setPlugPower(truePlug, value.as<bool>());
            ctx.setPlugPower(falsePlug, !value.as<bool>());
        }
    };

    class CounterState final : public NodeVirtualBase<CounterState>
    {
    public:
        static constexpr dsNodeTypeId typeId{dsHashFnv1a64("CounterState")};
        static constexpr dsNodeKind kind{dsNodeKind::State};

        static constexpr dsOutputSlot counterSlot = dsOutputSlot(0);
        static constexpr dsInputSlot incrementSlot = dsInputSlot(1);

        void onActivate(dsNodeContext& ctx) override
        {
            dsValueStorage value;
            if (!ctx.readOutputSlot(counterSlot, value.out()))
                value = dsValueStorage{0};
            if (!ctx.readSlot(incrementSlot, increment_.out()))
                increment_ = dsValueStorage{0};

            ctx.writeSlot(counterSlot, dsValueRef{value.as<int32_t>() + increment_.as<int32_t>()});
        }

        void onDeactivate(dsNodeContext& ctx) override
        {
            dsValueStorage value;
            if (!ctx.readOutputSlot(counterSlot, value.out()))
                value = dsValueStorage{0};
            ctx.writeSlot(counterSlot, dsValueRef{value.as<int32_t>() - increment_.as<int32_t>()});
        }

    private:
        dsValueStorage increment_;
    };

    class CanaryState final : public NodeVirtualBase<CanaryState>
    {
    public:
        static constexpr dsNodeTypeId typeId{dsHashFnv1a64("CanaryState")};
        static constexpr dsNodeKind kind{dsNodeKind::State};

        void onActivate(dsNodeContext& ctx) override { canaryValue = true; }
        void onDeactivate(dsNodeContext& ctx) override { canaryValue = false; }
    };

    class SetState final : public NodeVirtualBase<SetState>
    {
    public:
        static constexpr dsNodeTypeId typeId{dsHashFnv1a64("SetState")};
        static constexpr dsNodeKind kind{dsNodeKind::State};

        void onActivate(dsNodeContext& ctx) override { Update(ctx); }
        void onDependency(dsNodeContext& ctx) override { Update(ctx); }

    private:
        void Update(dsNodeContext& ctx)
        {
            dsValueStorage value;
            uint32_t const numSlots = ctx.numOutputSlots();
            for (uint8_t slotIndex = 0; slotIndex != numSlots; ++slotIndex)
            {
                if (ctx.readSlot(dsInputSlot(slotIndex), value.out()))
                    ctx.writeSlot(dsOutputSlot(slotIndex), value.ref());
            }
        }
    };

    class ToggleState final : public NodeVirtualBase<ToggleState>
    {
    public:
        static constexpr dsNodeTypeId typeId{dsHashFnv1a64("ToggleState")};
        static constexpr dsNodeKind kind{dsNodeKind::State};

        static constexpr dsInputPlugIndex togglePlug{0};

        static constexpr dsOutputPlugIndex enabledPlug{0};
        static constexpr dsOutputPlugIndex disabledPlug{0};

        void onActivate(dsNodeContext& ctx) override
        {
            ctx.setPlugPower(enabledPlug, toggled_);
            ctx.setPlugPower(disabledPlug, !toggled_);
        }
        void onCustomInput(dsNodeContext& ctx) override
        {
            toggled_ = !toggled_;
            ctx.setPlugPower(enabledPlug, toggled_);
            ctx.setPlugPower(disabledPlug, !toggled_);
        }

    private:
        bool toggled_ = false;
    };

    constexpr dsNodeTypeId entryNodeTypeId{dsHashFnv1a64("Entry")};

    static constexpr dsNodeCompileMeta nodes[] = {
        {.typeId = entryNodeTypeId, .kind = dsNodeKind::Entry},
        {.typeId = ConditionState::typeId, .kind = ConditionState::kind},
        {.typeId = CounterState::typeId, .kind = CounterState::kind},
        {.typeId = CanaryState::typeId, .kind = CanaryState::kind},
        {.typeId = SetState::typeId, .kind = SetState::kind},
        {.typeId = EmptyState::typeId, .kind = EmptyState::kind},
        {.typeId = ToggleState::typeId, .kind = ToggleState::kind},
    };

    static constexpr dsFunctionCompileMeta functions[] = {
        {.name = "series", .functionId = dsFunctionId{0}, .returnType = dsType<int32_t>.typeId},
        {.name = "readFlag", .functionId = dsFunctionId{1}, .returnType = dsType<bool>.typeId},
        {.name = "readFlagNum", .functionId = dsFunctionId{2}, .returnType = dsType<int32_t>.typeId},
    };

    class TestCompilerHost final : public dsGraphCompilerHost
    {
    public:
        explicit TestCompilerHost(dsAllocator& alloc) noexcept : errors_(alloc) {}

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
            uint32_t const functionLen = dsNameLen(name);

            for (dsFunctionCompileMeta const& meta : functions)
            {
                if (functionLen == std::strlen(meta.name) && 0 == std::strncmp(name.name, meta.name, functionLen))
                {
                    out_functionMeta = meta;
                    return true;
                }
            }
            return false;
        }

    private:
        dsArray<dsCompileError> errors_;
    };

    class TestRuntimeHost final : public dsRuntimeHost
    {
    public:
        TestRuntimeHost(dsAllocator& alloc, dsTypeDatabase& database) noexcept
            : allocator_(alloc), database_(database), nodes_(alloc), functions_(alloc)
        {
        }

        void registerNode(dsNodeTypeId typeId, dsNodeFunction function, uint32_t userSize, uint32_t userAlign);
        void registerFunction(dsFunctionId functionId, dsFunction function, void* userData = nullptr);

        template <typename NodeT>
        void registerNode()
        {
            registerNode(NodeT::typeId, NodeT::dispatch, sizeof(NodeT), alignof(NodeT));
        }

        bool lookupNode(dsNodeTypeId, dsNodeRuntimeMeta& out_meta) const noexcept override;
        bool lookupFunction(dsFunctionId, dsFunctionRuntimeMeta& out_meta) const noexcept override;
        bool lookupType(dsTypeId typeId, dsTypeMeta const*& out_meta) const noexcept override;

        dsAllocator& allocator() noexcept { return allocator_; }

    private:
        dsAllocator& allocator_;
        dsTypeDatabase& database_;
        dsArray<dsNodeRuntimeMeta> nodes_;
        dsArray<dsFunctionRuntimeMeta> functions_;
    };

    void TestRuntimeHost::registerNode(dsNodeTypeId typeId, dsNodeFunction function, uint32_t userSize, uint32_t userAlign)
    {
        nodes_.pushBack(dsNodeRuntimeMeta{.typeId = typeId, .function = function, .userSize = userSize, .userAlign = userAlign});
    }

    void TestRuntimeHost::registerFunction(dsFunctionId functionId, dsFunction function, void* userData)
    {
        functions_.pushBack(dsFunctionRuntimeMeta{.functionId = functionId, .function = function, .userData = userData});
    }

    bool TestRuntimeHost::lookupNode(dsNodeTypeId nodeTypeId, dsNodeRuntimeMeta& out_meta) const noexcept
    {
        for (uint32_t index = 0; index != nodes_.size(); ++index)
        {
            if (nodes_[index].typeId == nodeTypeId)
            {
                out_meta = nodes_[index];
                return true;
            }
        }
        return false;
    }

    bool TestRuntimeHost::lookupFunction(dsFunctionId functionId, dsFunctionRuntimeMeta& out_meta) const noexcept
    {
        for (uint32_t index = 0; index != functions_.size(); ++index)
        {
            if (functions_[index].functionId == functionId)
            {
                out_meta = functions_[index];
                return true;
            }
        }
        return false;
    }

    bool TestRuntimeHost::lookupType(dsTypeId typeId, dsTypeMeta const*& out_meta) const noexcept
    {
        dsTypeMeta const* const meta = database_.getMeta(typeId);
        if (meta != nullptr)
        {
            out_meta = meta;
            return true;
        }
        return false;
    }

    static void series(dsFunctionContext& ctx, void* userData)
    {
        int32_t result = 1;
        for (uint32_t index = 0; index != ctx.getArgCount(); ++index)
            result *= ctx.getArgAt<int32_t>(index);
        ctx.result(result);
    }

    static void readFlag(dsFunctionContext& ctx, void* userData)
    {
        ctx.listen(flagEmitterId);
        ctx.result(flagValue);
    }

    static void readFlagNum(dsFunctionContext& ctx, void* userData)
    {
        ctx.listen(flagEmitterId);
        ctx.result(flagValue ? 1 : 0);
    }
} // namespace

TEST_CASE("Graph Compiler", "[runtime]")
{
    using namespace descript;

    test::LeakTestAllocator alloc;

    dsTypeDatabase* database = dsCreateTypeDatabase(alloc);

    TestRuntimeHost runtimeHost(alloc, *database);

    runtimeHost.registerNode(entryNodeTypeId, nullptr, 0, alignof(void*));
    runtimeHost.registerNode<ConditionState>();
    runtimeHost.registerNode<CounterState>();
    runtimeHost.registerNode<CanaryState>();
    runtimeHost.registerNode<SetState>();
    runtimeHost.registerNode<EmptyState>();
    runtimeHost.registerNode<ToggleState>();

    runtimeHost.registerFunction(dsFunctionId{0}, series);
    runtimeHost.registerFunction(dsFunctionId{1}, readFlag);
    runtimeHost.registerFunction(dsFunctionId{2}, readFlagNum);

    TestCompilerHost compilerHost(alloc);
    dsGraphCompiler* compiler = dsCreateGraphCompiler(alloc, compilerHost);

    constexpr dsNodeId entryNodeId{0};
    constexpr dsNodeId setInitialNodeId{9834};
    constexpr dsNodeId conditionNodeId{2};
    constexpr dsNodeId counterNodeId{3};
    constexpr dsNodeId unusedNodeId{17};
    constexpr dsNodeId canaryNodeId{5};
    constexpr dsNodeId toggleNodeId{999};
    constexpr dsNodeId setResultNodeId{1790};
    constexpr dsNodeId setIncrementNodeId{2000};

    compiler->addVariable(dsType<int32_t>.typeId, "Scale");
    compiler->addVariable(dsType<int32_t>.typeId, "Count");
    compiler->addVariable(dsType<int32_t>.typeId, "Result");
    compiler->addVariable(dsType<int32_t>.typeId, "Increment");

    compiler->beginNode(entryNodeId, entryNodeTypeId);
    {
        compiler->addOutputPlug(dsDefaultOutputPlugIndex);
    }

    compiler->beginNode(setInitialNodeId, SetState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->addOutputPlug(dsDefaultOutputPlugIndex);

        compiler->beginInputSlot(dsInputSlot(0), dsType<int32_t>.typeId);
        {
            compiler->bindConstant(2);
        }
        compiler->beginOutputSlot(dsOutputSlot(0), dsType<int32_t>.typeId);
        {
            compiler->bindVariable("Scale");
        }
    }

    compiler->beginNode(conditionNodeId, ConditionState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->addOutputPlug(ConditionState::truePlug);
        compiler->addOutputPlug(ConditionState::falsePlug);
        compiler->beginInputSlot(ConditionState::conditionSlot, dsType<bool>.typeId);
        {
            compiler->bindExpression("readFlag()");
        }
    }

    compiler->beginNode(counterNodeId, CounterState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->beginOutputSlot(CounterState::counterSlot, dsType<int32_t>.typeId);
        {
            compiler->bindVariable("Count");
        }
        compiler->beginInputSlot(CounterState::incrementSlot, dsType<int32_t>.typeId);
        {
            compiler->bindExpression("series(2, 1, 2) + readFlagNum()");
        }
    }

    compiler->beginNode(unusedNodeId, EmptyState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
    }

    compiler->beginNode(canaryNodeId, CanaryState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
    }

    compiler->beginNode(toggleNodeId, ToggleState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->addInputPlug(ToggleState::togglePlug);
        compiler->addOutputPlug(ToggleState::enabledPlug);
        compiler->addOutputPlug(ToggleState::disabledPlug);
    }

    compiler->beginNode(setResultNodeId, SetState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->beginInputSlot(dsInputSlot(0), dsType<int32_t>.typeId);
        {
            compiler->bindExpression("Count * Scale");
        }
        compiler->beginOutputSlot(dsOutputSlot(0), dsType<int32_t>.typeId);
        {
            compiler->bindVariable("Result");
        }
    }

    compiler->beginNode(setIncrementNodeId, SetState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->beginInputSlot(dsInputSlot(0), dsType<int32_t>.typeId);
        {
            compiler->bindExpression("Increment + 1");
        }
        compiler->beginOutputSlot(dsOutputSlot(0), dsType<int32_t>.typeId);
        {
            compiler->bindVariable("Increment");
        }
    }

    compiler->addWire(entryNodeId, dsDefaultOutputPlugIndex, setInitialNodeId, dsBeginPlugIndex);
    compiler->addWire(setInitialNodeId, dsDefaultOutputPlugIndex, conditionNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::truePlug, counterNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::falsePlug, canaryNodeId, dsBeginPlugIndex);
    compiler->addWire(setInitialNodeId, dsDefaultOutputPlugIndex, toggleNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::truePlug, toggleNodeId, ToggleState::togglePlug);
    compiler->addWire(setInitialNodeId, dsDefaultOutputPlugIndex, setResultNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::truePlug, setIncrementNodeId, dsBeginPlugIndex);

    CHECK(compiler->compile());

    for (uint32_t errorIndex = 0; errorIndex != compiler->getErrorCount(); ++errorIndex)
        WARN((int)compiler->getError(errorIndex).code);
    REQUIRE(compiler->getErrorCount() == 0);

    REQUIRE(compiler->build());

    std::vector<uint8_t> blob(compiler->assemblyBytes(), compiler->assemblyBytes() + compiler->assemblySize());

    dsDestroyGraphCompiler(compiler);
    compiler = nullptr;

    dsRuntime* const runtime = dsCreateRuntime(alloc, runtimeHost);

    flagEmitterId = runtime->makeEmitterId();

    dsAssembly* assembly = dsLoadAssembly(alloc, runtimeHost, blob.data(), blob.size());
    REQUIRE(assembly != nullptr);

    dsParam const params[] = {
        {.name = dsName{"Count"}, .value = 0},
        {.name = dsName{"Increment"}, .value = 0},
    };

    flagValue = true;
    runtime->notifyChange(flagEmitterId);

    dsInstanceId instanceId = runtime->createInstance(assembly, params, sizeof(params) / sizeof(params[0]));
    REQUIRE(instanceId != dsInvalidInstanceId);

    dsReleaseAssembly(assembly);
    assembly = nullptr;

    auto readVar = [&](char const* variable) -> double {
        dsValueStorage value;
        if (!runtime->readVariable(instanceId, dsName{variable}, value.out()))
            return false;
        if (!value.is<int32_t>())
            return false;
        return value.as<int32_t>();
    };

    runtime->processEvents();

    CHECK(readVar("Count") == 5);
    CHECK(readVar("Result") == 10);
    CHECK(readVar("Increment") == 1);
    CHECK_FALSE(canaryValue);

    flagValue = false;
    runtime->notifyChange(flagEmitterId);
    runtime->processEvents();

    CHECK(readVar("Count") == 0);
    CHECK(readVar("Result") == 0);
    CHECK(readVar("Increment") == 1);
    CHECK(canaryValue);

    flagValue = true;
    runtime->notifyChange(flagEmitterId);
    runtime->processEvents();

    CHECK(readVar("Count") == 5);
    CHECK(readVar("Result") == 10);
    CHECK(readVar("Increment") == 2);
    CHECK_FALSE(canaryValue);

    flagValue = false;
    runtime->notifyChange(flagEmitterId);
    runtime->processEvents();

    CHECK(readVar("Count") == 0);
    CHECK(readVar("Result") == 0);
    CHECK(readVar("Increment") == 2);
    CHECK(canaryValue);

    runtime->destroyInstance(instanceId);

    runtime->processEvents();

    CHECK_FALSE(canaryValue);

    dsDestroyRuntime(runtime);
    dsDestroyTypeDatabase(database);
}
