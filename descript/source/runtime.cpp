// descript

#include "descript/runtime.hh"

#include "descript/assembly.hh"
#include "descript/context.hh"
#include "descript/evaluate.hh"
#include "descript/value.hh"

#include "array.hh"
#include "assembly_internal.hh"
#include "fnv.hh"
#include "instance.hh"

namespace descript {
    namespace {
        class Runtime : public dsRuntime
        {
        public:
            Runtime(dsAllocator& alloc, dsRuntimeHost& host) noexcept;
            ~Runtime();

            Runtime(Runtime const&) = delete;
            Runtime& operator=(Runtime const&) = delete;

            dsInstanceId createInstance(dsAssembly* assembly, dsParam const* params, uint32_t paramCount) override;
            void destroyInstance(dsInstanceId instanceId) override;

            bool writeVariable(dsInstanceId instanceId, dsName name, dsValueRef const& value) override;
            bool readVariable(dsInstanceId instanceId, dsName name, dsValueOut out_value) override;

            void processEvents() override;

            dsEmitterId makeEmitterId() override;
            void notifyChange(dsEmitterId emitterId) override;

            dsAllocator& allocator() noexcept { return allocator_; }

        private:
            class Context;
            class EvaluateHost;

            struct Listener
            {
                dsInstanceId instanceId = dsInvalidInstanceId;
                dsEmitterId emitterId = dsInvalidEmitterId;
                uint32_t inputSlotIndex = 0;
            };

            dsInstance* findInstance(dsInstanceId instanceId) noexcept;

            void deleteInstance(dsInstance* instance);

            void sendLocalEvent(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsEvent const& event);

            void processEvents(dsInstance& instance);
            void processEvent(dsInstance& instance, dsAssemblyNodeIndex, dsEvent const& event);
            void dispatchEvent(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsEvent const& event);

            void setPlugPower(dsInstance& instance, dsAssemblyOutputPlugIndex plugIndex, bool powered);
            void setPlugPower(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsOutputPlugIndex plugIndex, bool powered);

            void setNodePowered(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, bool powered);

            bool readSlot(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsInputSlotIndex slotIndex, dsValueOut out_value);
            bool readSlot(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsOutputSlotIndex slotIndex, dsValueOut out_value);
            bool writeSlot(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsOutputSlotIndex slotIndex, dsValueRef const& value);

            bool writeVariable(dsInstance& instance, uint64_t nameHash, dsValueRef const& value);
            void writeVariable(dsInstance& instance, dsAssemblyVariableIndex variableIndex, dsAssemblyNodeIndex sourceNodeIndex,
                dsValueRef const& value);

            void addListener(dsInstanceId instanceId, uint32_t inputSlotIndex, dsEmitterId emitterId);
            void forgetListener(dsInstanceId instanceId, uint32_t inputSlotIndex);
            void forgetListener(dsInstanceId instanceId);

            void triggerDependencies(dsInstance& instance, dsAssemblyVariableIndex variableIndex, dsAssemblyNodeIndex sourceNodeIndex);
            void triggerChange(dsInstanceId instanceId, uint32_t inputSlotIndex);

            dsAllocator& allocator_;
            dsArray<dsInstance*> instances_;
            dsArray<Listener> listeners_;
            uint32_t nextInstanceId_ = 0;
            uint64_t nextEmitterId_ = 0;
        };

        Runtime::Runtime(dsAllocator& alloc, dsRuntimeHost& host) noexcept : allocator_(alloc), instances_(alloc), listeners_(alloc) {}

        Runtime::~Runtime()
        {
            for (dsInstance* instance : instances_)
                deleteInstance(instance);
        }
    } // namespace

    dsRuntime* dsCreateRuntime(dsAllocator& alloc, dsRuntimeHost& host)
    {
        return new (alloc.allocate(sizeof(Runtime), alignof(Runtime))) Runtime(alloc, host);
    }

    void dsDestroyRuntime(dsRuntime* runtime)
    {
        if (runtime != nullptr)
        {
            Runtime* impl = static_cast<Runtime*>(runtime);
            dsAllocator& alloc = impl->allocator();
            impl->~Runtime();
            alloc.free(impl, sizeof(Runtime), alignof(Runtime));
        }
    }

    dsInstanceId Runtime::createInstance(dsAssembly* assembly, dsParam const* params, uint32_t paramCount)
    {
        DS_GUARD_OR(assembly != nullptr, dsInvalidInstanceId);
        DS_GUARD_OR(params != nullptr || paramCount == 0, dsInvalidInstanceId);

        dsAcquireAssembly(assembly);

        dsAssemblyHeader const& header = *assembly->header;

        void* const memory = allocator_.allocate(assembly->instanceSize, alignof(dsInstance));
        std::memset(memory, 0, assembly->instanceSize);

        dsInstanceId const instanceId{nextInstanceId_++};

        dsInstance& instance = *instances_.pushBack(new (memory) dsInstance(allocator_, instanceId));
        instance.assembly = assembly;

        instance.activeNodes.assign(reinterpret_cast<uintptr_t>(&instance), assembly->instanceStatesOffset, header.nodes.count);
        instance.activeInputPlugs.assign(reinterpret_cast<uintptr_t>(&instance), assembly->instanceInputPlugsOffset, header.inputPlugCount);
        instance.activeOutputPlugs.assign(reinterpret_cast<uintptr_t>(&instance), assembly->instanceOutputPlugsOffset,
            header.outputPlugs.count);
        instance.values.assign(reinterpret_cast<uintptr_t>(&instance), assembly->instanceValuesOffset, header.variables.count);

        // reset all variables, since the memset will leave them in an invalid state
        for (uint32_t index = 0; index != instance.values.count; ++index)
            instance.values[dsAssemblyVariableIndex(index)] = {};

        for (uint32_t index = 0; index != paramCount; ++index)
            writeVariable(instance, dsHashFnv1a64(params[index].name.name, params[index].name.nameEnd), dsValueRef{params[index].value});

        for (dsAssemblyNodeIndex nodeIndex : header.entryNodes)
            sendLocalEvent(instance, nodeIndex, {.type = dsEventType::Activate});

        return instance.instanceId;
    }

    void Runtime::destroyInstance(dsInstanceId instanceId)
    {
        for (dsInstance*& instance : instances_)
        {
            if (instance != nullptr && instance->instanceId == instanceId)
            {
                // immediately deactivate all nodes
                for (dsAssemblyNodeIndex nodeIndex{0}; nodeIndex != instance->activeNodes.count; ++nodeIndex)
                    if (instance->activeNodes[nodeIndex])
                        dispatchEvent(*instance, nodeIndex, {.type = dsEventType::Deactivate});

                deleteInstance(instance);
                instance = nullptr;
                break;
            }
        }
    }

    bool Runtime::writeVariable(dsInstanceId instanceId, dsName name, dsValueRef const& value)
    {
        dsInstance* const instance = findInstance(instanceId);
        if (instance == nullptr)
            return false;

        uint64_t const nameHash = dsHashFnv1a64(name.name, name.nameEnd);

        return writeVariable(*instance, nameHash, value);
    }

    bool Runtime::readVariable(dsInstanceId instanceId, dsName variable, dsValueOut out_value)
    {
        dsInstance* const instance = findInstance(instanceId);
        if (instance == nullptr)
            return false;

        uint64_t const variableHash = dsHashFnv1a64(variable.name, variable.nameEnd);

        dsAssemblyHeader const& header = *instance->assembly->header;
        for (dsAssemblyVariableIndex variableIndex{0}; variableIndex != header.variables.count; ++variableIndex)
            if (header.variables[variableIndex].nameHash == variableHash)
                return out_value.accept(instance->values[variableIndex].ref());

        return false;
    }

    void Runtime::processEvents()
    {
        for (dsInstance* const instance : instances_)
            if (instance != nullptr)
                processEvents(*instance);
    }

    dsEmitterId Runtime::makeEmitterId() { return dsEmitterId{nextEmitterId_++}; }

    void Runtime::addListener(dsInstanceId instanceId, uint32_t inputSlotIndex, dsEmitterId emitterId)
    {
        DS_GUARD_VOID(instanceId != dsInvalidInstanceId);
        DS_GUARD_VOID(emitterId != dsInvalidEmitterId);

        for (Listener& listener : listeners_)
            if (listener.instanceId == instanceId && listener.emitterId == emitterId && listener.inputSlotIndex == inputSlotIndex)
                return;

        for (Listener& listener : listeners_)
        {
            if (listener.instanceId == dsInvalidInstanceId || listener.emitterId == dsInvalidEmitterId)
            {
                listener.instanceId = instanceId;
                listener.emitterId = emitterId;
                listener.inputSlotIndex = inputSlotIndex;
                return;
            }
        }

        listeners_.pushBack(Listener{.instanceId = instanceId, .emitterId = emitterId, .inputSlotIndex = inputSlotIndex});
    }

    void Runtime::forgetListener(dsInstanceId instanceId, uint32_t inputSlotIndex)
    {
        DS_GUARD_VOID(instanceId != dsInvalidInstanceId);

        for (Listener& listener : listeners_)
            if (listener.instanceId == instanceId && listener.inputSlotIndex == inputSlotIndex)
                listener = Listener{};
    }

    void Runtime::forgetListener(dsInstanceId instanceId)
    {
        DS_GUARD_VOID(instanceId != dsInvalidInstanceId);

        for (Listener& listener : listeners_)
            if (listener.instanceId == instanceId)
                listener = Listener{};
    }

    void Runtime::notifyChange(dsEmitterId emitterId)
    {
        DS_GUARD_VOID(emitterId != dsInvalidEmitterId);

        for (Listener& listener : listeners_)
            if (listener.emitterId == emitterId && listener.instanceId != dsInvalidInstanceId)
                triggerChange(listener.instanceId, listener.inputSlotIndex);
    }

    dsInstance* Runtime::findInstance(dsInstanceId instanceId) noexcept
    {
        for (dsInstance* const instance : instances_)
            if (instance != nullptr && instance->instanceId == instanceId)
                return instance;

        return nullptr;
    }

    class Runtime::Context final : public dsNodeContext
    {
    public:
        Context(Runtime& runtime, dsInstance& instance, dsAssemblyNodeIndex nodeIndex) noexcept
            : runtime_(runtime), instance_(instance), nodeIndex_(nodeIndex)
        {
        }

        dsInstanceId instanceId() const noexcept override { return instance_.instanceId; }
        dsNodeIndex nodeIndex() const noexcept override { return dsNodeIndex{nodeIndex_.value()}; }

        uint32_t numInputPlugs() const noexcept override;
        uint32_t numOutputPlugs() const noexcept override;
        uint32_t numInputSlots() const noexcept override;
        uint32_t numOutputSlots() const noexcept override;

        bool readSlot(dsInputSlotIndex slotIndex, dsValueOut out_value) override;
        bool readSlot(dsOutputSlotIndex slotIndex, dsValueOut out_value) override;

        void writeSlot(dsOutputSlotIndex slotIndex, dsValueRef const& value) override;

        void setPlugPower(dsOutputPlugIndex plugIndex, bool powered) override;

    private:
        Runtime& runtime_;
        dsInstance& instance_;
        dsAssemblyNodeIndex nodeIndex_;
    };

    uint32_t Runtime::Context::numInputPlugs() const noexcept
    {
        dsAssemblyHeader const& header = *instance_.assembly->header;
        return header.nodes[nodeIndex_].customInputPlugCount;
    }

    uint32_t Runtime::Context::numOutputPlugs() const noexcept
    {
        dsAssemblyHeader const& header = *instance_.assembly->header;
        return header.nodes[nodeIndex_].customOutputPlugCount;
    }

    uint32_t Runtime::Context::numInputSlots() const noexcept
    {
        dsAssemblyHeader const& header = *instance_.assembly->header;
        return header.nodes[nodeIndex_].inputSlotCount;
    }

    uint32_t Runtime::Context::numOutputSlots() const noexcept
    {
        dsAssemblyHeader const& header = *instance_.assembly->header;
        return header.nodes[nodeIndex_].outputSlotCount;
    }

    bool Runtime::Context::readSlot(dsInputSlotIndex slotIndex, dsValueOut out_value)
    {
        return runtime_.readSlot(instance_, nodeIndex_, slotIndex, out_value);
    }

    bool Runtime::Context::readSlot(dsOutputSlotIndex slotIndex, dsValueOut out_value)
    {
        return runtime_.readSlot(instance_, nodeIndex_, slotIndex, out_value);
    }

    void Runtime::Context::writeSlot(dsOutputSlotIndex slotIndex, dsValueRef const& value)
    {
        runtime_.writeSlot(instance_, nodeIndex_, slotIndex, value);
    }

    void Runtime::Context::setPlugPower(dsOutputPlugIndex plugIndex, bool powered)
    {
        runtime_.setPlugPower(instance_, nodeIndex_, plugIndex, powered);
    }

    class Runtime::EvaluateHost final : public dsEvaluateHost
    {
    public:
        EvaluateHost(Runtime& runtime, dsInstance& instance, dsAssemblyInputSlotIndex inputSlotIndex) noexcept
            : runtime_(runtime), instance_(instance), inputSlotIndex_(inputSlotIndex)
        {
        }

        void listen(dsEmitterId emitterId) override { runtime_.addListener(instance_.instanceId, inputSlotIndex_.value(), emitterId); }

        bool readConstant(uint32_t constantIndex, dsValueOut out_value) override;
        bool readVariable(uint32_t variableIndex, dsValueOut out_value) override;
        bool invokeFunction(uint32_t functionIndex, dsFunctionContext& ctx) override;

    private:
        Runtime& runtime_;
        dsInstance& instance_;
        dsSpan<dsValueStorage const> constants_;
        dsSpan<dsValueStorage const> variables_;
        dsAssemblyInputSlotIndex inputSlotIndex_ = dsInvalidIndex;
    };

    bool Runtime::EvaluateHost::readConstant(uint32_t constantIndex, dsValueOut out_value)
    {
        if (constantIndex < instance_.assembly->constants.count)
            return out_value.accept(instance_.assembly->constants[dsAssemblyConstantIndex{constantIndex}].ref());
        return false;
    }

    bool Runtime::EvaluateHost::readVariable(uint32_t variableIndex, dsValueOut out_value)
    {
        if (variableIndex < instance_.values.count)
            return out_value.accept(instance_.values[dsAssemblyVariableIndex{variableIndex}].ref());
        return false;
    }

    bool Runtime::EvaluateHost::invokeFunction(uint32_t functionIndex, dsFunctionContext& ctx)
    {
        if (functionIndex < instance_.assembly->functions.count)
        {
            dsAssemblyFunctionImpl const& func = instance_.assembly->functions[dsAssemblyFunctionIndex{functionIndex}];
            func.function(ctx, func.userData);
            return true;
        }
        return false;
    }

    void Runtime::deleteInstance(dsInstance* instance)
    {
        if (instance != nullptr)
        {
            uint32_t const size = instance->assembly->instanceSize;

            dsReleaseAssembly(instance->assembly);
            forgetListener(instance->instanceId);

            instance->~dsInstance();
            allocator_.free(instance, size, alignof(dsInstance));
        }
    }

    void Runtime::sendLocalEvent(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsEvent const& event)
    {
        DS_ASSERT(nodeIndex.value() < instance.assembly->header->nodes.count);

        instance.events.pushBack(dsInstance::Event{.nodeIndex = nodeIndex, .event = event});
    }

    void Runtime::processEvents(dsInstance& instance)
    {
        for (uint32_t eventIndex = 0; eventIndex != instance.events.size(); ++eventIndex)
        {
            // make a copy so the reference does not become invalid in the callback if
            // a new event is pushed.
            dsInstance::Event const& event = instance.events[eventIndex];
            processEvent(instance, event.nodeIndex, event.event);
        }

        instance.events.clear();
    }

    void Runtime::processEvent(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsEvent const& event)
    {
        switch (event.type)
        {
        case dsEventType::Activate:
            if (instance.activeNodes[nodeIndex])
                break;

            instance.activeNodes.set(nodeIndex);
            dispatchEvent(instance, nodeIndex, event);
            setPlugPower(instance, nodeIndex, dsDefaultOutputPlugIndex, true);
            break;
        case dsEventType::Deactivate: {
            if (!instance.activeNodes[nodeIndex])
                break;

            instance.activeNodes.clear(nodeIndex);
            dispatchEvent(instance, nodeIndex, event);

            dsAssemblyHeader const& header = *instance.assembly->header;
            dsAssemblyNode const& node = header.nodes[nodeIndex];

            // depower the main output plug
            if (node.outputPlug != dsInvalidIndex)
                setPlugPower(instance, node.outputPlug, false);

            // depower any custom output plugs
            for (uint32_t plugIndex = node.customOutputPlugStart.value(), lastIndex = plugIndex + node.customOutputPlugCount;
                 plugIndex != lastIndex; ++plugIndex)
            {
                setPlugPower(instance, dsAssemblyOutputPlugIndex{plugIndex}, false);
            }
            break;
        }
        case dsEventType::Dependency:
            if (!instance.activeNodes[nodeIndex])
                break;
            dispatchEvent(instance, nodeIndex, event);
            break;
        case dsEventType::CustomInput:
            if (!instance.activeNodes[nodeIndex])
                break;
            dispatchEvent(instance, nodeIndex, event);
            break;
        }
    }

    void Runtime::dispatchEvent(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsEvent const& event)
    {
        DS_ASSERT(nodeIndex.value() < instance.activeNodes.count);

        dsAssemblyNodeImpl const impl = instance.assembly->nodes[nodeIndex];
        Context context(*this, instance, nodeIndex);
        impl.function(context, event.type, reinterpret_cast<uint8_t*>(&instance) + impl.userOffset);
    }

    void Runtime::setPlugPower(dsInstance& instance, dsAssemblyOutputPlugIndex plugIndex, bool powered)
    {
        DS_ASSERT(plugIndex.value() < instance.activeOutputPlugs.count);

        if (instance.activeOutputPlugs[plugIndex] == powered)
            return;

        if (powered)
            instance.activeOutputPlugs.set(plugIndex);
        else
            instance.activeOutputPlugs.clear(plugIndex);

        dsAssemblyHeader const& header = *instance.assembly->header;
        dsAssemblyOutputPlug const& plug = header.outputPlugs[plugIndex];

        for (dsAssemblyWireIndex wireIndex = plug.wireStart, lastIndex = plug.wireStart + plug.wireCount; wireIndex != lastIndex;
             ++wireIndex)
        {
            dsAssemblyWire const& wire = header.wires[wireIndex];
            if (wire.inputPlugIndex == dsBeginPlugIndex)
            {
                setNodePowered(instance, wire.nodeIndex, powered);
            }
            else if (wire.inputPlugIndex != dsInputPlugIndex{0xFF})
            {
                sendLocalEvent(instance, wire.nodeIndex,
                    {.data = {.input = {.inputPlugIndex = wire.inputPlugIndex}}, .type = dsEventType::CustomInput});
            }
        }
    }

    void Runtime::setPlugPower(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsOutputPlugIndex plugIndex, bool powered)
    {
        DS_ASSERT(nodeIndex.value() < instance.activeNodes.count);
        DS_ASSERT(instance.activeNodes[nodeIndex]);

        dsAssemblyHeader const& header = *instance.assembly->header;
        dsAssemblyNode const& node = header.nodes[nodeIndex];

        if (plugIndex == dsDefaultOutputPlugIndex && node.outputPlug != dsInvalidIndex)
            setPlugPower(instance, node.outputPlug, powered);
        else if (plugIndex.value() < node.customOutputPlugCount)
            setPlugPower(instance, node.customOutputPlugStart + plugIndex.value(), powered);
    }

    void Runtime::setNodePowered(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, bool powered)
    {
        if (powered)
            sendLocalEvent(instance, nodeIndex, {.type = dsEventType::Activate});
        else
            sendLocalEvent(instance, nodeIndex, {.type = dsEventType::Deactivate});
    }

    bool Runtime::readSlot(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsInputSlotIndex slotIndex, dsValueOut out_value)
    {
        DS_ASSERT(nodeIndex.value() < instance.activeNodes.count);

        dsAssembly const& assembly = *instance.assembly;
        dsAssemblyHeader const& header = *assembly.header;
        dsAssemblyNode const& node = header.nodes[nodeIndex];
        if (slotIndex.value() >= node.inputSlotCount)
            return false;

        dsAssemblyInputSlotIndex const inputSlotIndex = node.inputSlotStart + slotIndex.value();
        dsAssemblyInputSlot const& slot = header.inputSlots[inputSlotIndex];

        if (slot.variableIndex != dsInvalidIndex)
            return out_value.accept(instance.values[slot.variableIndex].ref());

        if (slot.constantIndex != dsInvalidIndex)
            return out_value.accept(assembly.constants[slot.constantIndex].ref());

        if (slot.expressionIndex != dsInvalidIndex)
        {
            dsAssemblyExpression const& expression = header.expressions[slot.expressionIndex];

            forgetListener(instance.instanceId, inputSlotIndex.value());

            EvaluateHost host(*this, instance, inputSlotIndex);
            if (!dsEvaluate(host, header.byteCode.base.get() + expression.codeStart.value(), expression.codeCount, out_value))
                return false;
            return true;
        }

        return false;
    }

    bool Runtime::readSlot(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsOutputSlotIndex slotIndex, dsValueOut out_value)
    {
        DS_ASSERT(nodeIndex.value() < instance.activeNodes.count);

        dsAssemblyHeader const& header = *instance.assembly->header;
        dsAssemblyNode const& node = header.nodes[nodeIndex];
        if (slotIndex.value() >= node.inputSlotCount)
            return false;

        dsAssemblyOutputSlot const& slot = header.outputSlots[node.outputSlotStart + slotIndex.value()];
        if (slot.variableIndex != dsInvalidIndex)
            return out_value.accept(instance.values[slot.variableIndex].ref());

        return false;
    }

    bool Runtime::writeSlot(dsInstance& instance, dsAssemblyNodeIndex nodeIndex, dsOutputSlotIndex slotIndex, dsValueRef const& value)
    {
        DS_ASSERT(nodeIndex.value() < instance.activeNodes.count);

        dsAssemblyHeader const& header = *instance.assembly->header;
        dsAssemblyNode const& node = header.nodes[nodeIndex];
        if (slotIndex.value() >= node.outputSlotCount)
            return false;

        dsAssemblyOutputSlot const& slot = header.outputSlots[node.outputSlotStart + slotIndex.value()];
        if (slot.variableIndex == dsInvalidIndex)
            return false;

        writeVariable(instance, slot.variableIndex, nodeIndex, value);
        return true;
    }

    bool Runtime::writeVariable(dsInstance& instance, uint64_t nameHash, dsValueRef const& value)
    {
        dsAssemblyHeader const& header = *instance.assembly->header;
        for (dsAssemblyVariableIndex variableIndex{0}; variableIndex != header.variables.count; ++variableIndex)
        {
            if (header.variables[variableIndex].nameHash == nameHash)
            {
                writeVariable(instance, variableIndex, dsInvalidIndex, value);
                return true;
            }
        }

        return false;
    }

    void Runtime::writeVariable(dsInstance& instance, dsAssemblyVariableIndex variableIndex, dsAssemblyNodeIndex sourceNodeIndex,
        dsValueRef const& value)
    {
        DS_ASSERT(variableIndex.value() < instance.values.count);

        dsAssemblyHeader const& header = *instance.assembly->header;
        if (instance.values[variableIndex] != value)
        {
            instance.values[variableIndex] = dsValueStorage{value};
            triggerDependencies(instance, variableIndex, sourceNodeIndex);
        }
    }

    void Runtime::triggerDependencies(dsInstance& instance, dsAssemblyVariableIndex variableIndex, dsAssemblyNodeIndex sourceNodeIndex)
    {
        DS_ASSERT(variableIndex.value() < instance.values.count);

        dsAssemblyHeader const& header = *instance.assembly->header;
        dsAssemblyVariable const& var = header.variables[variableIndex];

        for (dsAssemblyDependencyIndex depIndex = var.dependencyStart, lastIndex = depIndex + var.dependencyCount; depIndex != lastIndex;
             ++depIndex)
        {
            dsAssemblyDependency const& dependency = header.dependencies[depIndex];

            // avoid a write from a node triggering itself
            if (dependency.nodeIndex == sourceNodeIndex)
                continue;

            // we can immediately check if nodes are active here; if a node has a pending activation,
            // then it will read the up-to-date value when it activates, and the update would be superfluous
            if (!instance.activeNodes[dependency.nodeIndex])
                continue;

            sendLocalEvent(instance, dependency.nodeIndex, {.type = dsEventType::Dependency});
        }
    }

    void Runtime::triggerChange(dsInstanceId instanceId, uint32_t inputSlotIndex)
    {
        dsInstance* instance = findInstance(instanceId);
        if (instance == nullptr)
            return;

        DS_GUARD_VOID(inputSlotIndex < instance->assembly->header->inputSlots.count);
        dsAssemblyInputSlot const& inputSlot = instance->assembly->header->inputSlots[dsAssemblyInputSlotIndex{inputSlotIndex}];

        DS_GUARD_VOID(inputSlot.nodeIndex != dsInvalidIndex);
        sendLocalEvent(*instance, inputSlot.nodeIndex, dsEvent{.type = dsEventType::Dependency});
    }

} // namespace descript
