// descript

#include <catch_amalgamated.hpp>

#include "descript/alloc.hh"
#include "descript/assembly.hh"
#include "descript/context.hh"
#include "descript/evaluate.hh"
#include "descript/graph_compiler.hh"
#include "descript/node.hh"
#include "descript/runtime.hh"

#include "array.hh"
#include "fnv.hh"
#include "leak_alloc.hh"
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

        static constexpr dsInputSlotIndex conditionSlot{0};

        void onActivate(dsNodeContext& ctx) override { Update(ctx); }
        void onDependency(dsNodeContext& ctx) override { Update(ctx); }

    private:
        void Update(dsNodeContext& ctx)
        {
            dsValue value;
            if (!ctx.readSlot(conditionSlot, value))
                value = dsValue{false};
            ctx.setPlugPower(truePlug, value.as<bool>());
            ctx.setPlugPower(falsePlug, !value.as<bool>());
        }
    };

    class CounterState final : public NodeVirtualBase<CounterState>
    {
    public:
        static constexpr dsNodeTypeId typeId{dsHashFnv1a64("CounterState")};
        static constexpr dsNodeKind kind{dsNodeKind::State};

        static constexpr dsOutputSlotIndex counterSlot{0};
        static constexpr dsInputSlotIndex incrementSlot{1};

        void onActivate(dsNodeContext& ctx) override
        {
            dsValue value;
            if (!ctx.readSlot(counterSlot, value))
                value = dsValue{0};
            if (!ctx.readSlot(incrementSlot, increment_))
                increment_ = dsValue{0};

            ctx.writeSlot(counterSlot, dsValue{value.as<int32_t>() + increment_.as<int32_t>()});
        }

        void onDeactivate(dsNodeContext& ctx) override
        {
            dsValue value;
            if (!ctx.readSlot(counterSlot, value))
                value = dsValue{0};
            ctx.writeSlot(counterSlot, dsValue{value.as<int32_t>() - increment_.as<int32_t>()});
        }

    private:
        dsValue increment_;
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
            dsValue value;
            uint32_t const numSlots = ctx.numOutputSlots();
            for (uint8_t slotIndex = 0; slotIndex != numSlots; ++slotIndex)
            {
                if (ctx.readSlot(dsInputSlotIndex{slotIndex}, value))
                    ctx.writeSlot(dsOutputSlotIndex{(uint8_t)(slotIndex)}, value);
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
        {.name = "series", .functionId = dsFunctionId{0}, .returnType = dsValueType::Int32},
        {.name = "readFlag", .functionId = dsFunctionId{1}, .returnType = dsValueType::Bool},
        {.name = "readFlagNum", .functionId = dsFunctionId{2}, .returnType = dsValueType::Int32},
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
        TestRuntimeHost(dsAllocator& alloc) noexcept : allocator_(alloc), nodes_(alloc), functions_(alloc) {}

        void registerNode(dsNodeTypeId typeId, dsNodeFunction function, uint32_t userSize, uint32_t userAlign);
        void registerFunction(dsFunctionId functionId, dsFunction function, void* userData = nullptr);

        template <typename NodeT>
        void registerNode()
        {
            registerNode(NodeT::typeId, NodeT::dispatch, sizeof(NodeT), alignof(NodeT));
        }

        bool lookupNode(dsNodeTypeId, dsNodeRuntimeMeta& out_meta) const noexcept override;
        bool lookupFunction(dsFunctionId, dsFunctionRuntimeMeta& out_meta) const noexcept override;

        dsAllocator& allocator() noexcept { return allocator_; }

    private:
        dsAllocator& allocator_;
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

    static dsValue series(dsFunctionContext& ctx, void* userData)
    {
        int32_t result = 1;
        for (uint32_t index = 0; index != ctx.argc(); ++index)
            result *= ctx.argAt(index).as<int32_t>();
        return dsValue{result};
    }

    static dsValue readFlag(dsFunctionContext& ctx, void* userData)
    {
        ctx.listen(flagEmitterId);
        return dsValue{flagValue};
    }

    static dsValue readFlagNum(dsFunctionContext& ctx, void* userData)
    {
        ctx.listen(flagEmitterId);
        return dsValue{flagValue ? 1 : 0};
    }
} // namespace

TEST_CASE("Graph Compiler", "[runtime]")
{
    using namespace descript;

    test::LeakTestAllocator alloc;

    TestRuntimeHost runtimeHost(alloc);

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
    constexpr dsNodeId conditionNodeId{2};
    constexpr dsNodeId counterNodeId{3};
    constexpr dsNodeId unusedNodeId{17};
    constexpr dsNodeId canaryNodeId{5};
    constexpr dsNodeId toggleNodeId{999};
    constexpr dsNodeId setResultNodeId{1790};
    constexpr dsNodeId setIncrementNodeId{2000};

    compiler->addVariable(dsValueType::Int32, "Count");
    compiler->addVariable(dsValueType::Int32, "Result");
    compiler->addVariable(dsValueType::Int32, "Increment");

    compiler->beginNode(entryNodeId, entryNodeTypeId);
    {
        compiler->addOutputPlug(dsDefaultOutputPlugIndex);
    }

    compiler->beginNode(conditionNodeId, ConditionState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->addOutputPlug(ConditionState::truePlug);
        compiler->addOutputPlug(ConditionState::falsePlug);
        compiler->addInputSlot(ConditionState::conditionSlot);
    }

    compiler->beginNode(counterNodeId, CounterState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->addOutputSlot(CounterState::counterSlot);
        compiler->addInputSlot(CounterState::incrementSlot);
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
        compiler->addOutputSlot(dsOutputSlotIndex{0});
        compiler->addInputSlot(dsInputSlotIndex{0});
    }

    compiler->beginNode(setIncrementNodeId, SetState::typeId);
    {
        compiler->addInputPlug(dsBeginPlugIndex);
        compiler->addOutputSlot(dsOutputSlotIndex{0});
        compiler->addInputSlot(dsInputSlotIndex{0});
    }

    compiler->bindSlotExpression(conditionNodeId, ConditionState::conditionSlot, "readFlag()");
    compiler->bindOutputSlotVariable(counterNodeId, CounterState::counterSlot, "Count");
    compiler->bindSlotExpression(counterNodeId, CounterState::incrementSlot, "series(2, 1, 2) + readFlagNum()");
    compiler->bindSlotExpression(setResultNodeId, dsInputSlotIndex{0}, "Count * 2");
    compiler->bindOutputSlotVariable(setResultNodeId, dsOutputSlotIndex{0}, "Result");
    compiler->bindSlotExpression(setIncrementNodeId, dsInputSlotIndex{0}, "Increment + 1");
    compiler->bindOutputSlotVariable(setIncrementNodeId, dsOutputSlotIndex{0}, "Increment");

    compiler->addWire(entryNodeId, dsDefaultOutputPlugIndex, conditionNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::truePlug, counterNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::falsePlug, canaryNodeId, dsBeginPlugIndex);
    compiler->addWire(entryNodeId, dsDefaultOutputPlugIndex, toggleNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::truePlug, toggleNodeId, ToggleState::togglePlug);
    compiler->addWire(entryNodeId, dsDefaultOutputPlugIndex, setResultNodeId, dsBeginPlugIndex);
    compiler->addWire(conditionNodeId, ConditionState::truePlug, setIncrementNodeId, dsBeginPlugIndex);

    REQUIRE(compiler->compile());
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
        dsValue value;
        if (!runtime->readVariable(instanceId, dsName{variable}, value))
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
}
