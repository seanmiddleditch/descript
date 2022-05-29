// descript

#include "descript/assembly.hh"
#include "descript/alloc.hh"
#include "descript/runtime.hh"
#include "descript/value.hh"

#include "assembly_internal.hh"

#include "bit.hh"
#include "fnv.hh"
#include "instance.hh"

#include <new>

namespace descript {
    static bool isInRange(uint32_t offset, uint32_t count, uint32_t range) noexcept { return offset <= range && count <= range - offset; }

#define DS_VALIDATE(x) \
    if (!(x))          \
    {                  \
        DS_BREAK();    \
        return false;  \
    }

    bool dsValidateAssembly(uint8_t const* bytes, uint32_t size) noexcept
    {
        // ensure the byte range is valid and at least large enough for the header
        DS_VALIDATE(bytes != nullptr);
        DS_VALIDATE(size >= sizeof(dsAssemblyHeader));

        dsAssemblyHeader const& header = *std::launder(reinterpret_cast<dsAssemblyHeader const*>(bytes));

        // ensure the block is big enough for the header's declared size
        DS_VALIDATE(size >= header.size);

        // validate the hash
        DS_VALIDATE(dsHashAssembly(&header) == header.hash);

        // ensure embedded arrays are encloded in the block
        DS_VALIDATE(header.nodes.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.entryNodes.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.outputPlugs.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.wires.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.inputSlots.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.outputSlots.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.variables.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.dependencies.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.expressions.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.constants.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.functions.validate(reinterpret_cast<uintptr_t>(bytes), header.size));
        DS_VALIDATE(header.byteCode.validate(reinterpret_cast<uintptr_t>(bytes), header.size));

        // validate all cross-references indices and ranges
        for (dsAssemblyNode const& node : header.nodes)
        {
            DS_VALIDATE(isInRange(node.customOutputPlugStart.value(), node.customOutputPlugCount, header.outputPlugs.count));
            DS_VALIDATE(isInRange(node.inputSlotStart.value(), node.inputSlotCount, header.inputSlots.count));
            DS_VALIDATE(isInRange(node.outputSlotStart.value(), node.outputSlotCount, header.outputSlots.count));

            DS_VALIDATE(node.outputPlug == dsInvalidIndex || node.outputPlug.value() < header.outputPlugs.count);
        }

        for (dsAssemblyNodeIndex entryIndex : header.entryNodes)
            DS_VALIDATE(entryIndex.value() < header.nodes.count);

        for (dsAssemblyOutputPlug const& outputPlug : header.outputPlugs)
            DS_VALIDATE(isInRange(outputPlug.wireStart.value(), outputPlug.wireCount, header.wires.count));

        for (dsAssemblyWire const& wire : header.wires)
            DS_VALIDATE(wire.nodeIndex.value() < header.nodes.count);

        for (dsAssemblyInputSlot const& slot : header.inputSlots)
        {
            if (slot.variableIndex != dsInvalidIndex)
            {
                DS_VALIDATE(slot.expressionIndex == dsInvalidIndex);
                DS_VALIDATE(slot.variableIndex.value() < header.variables.count);
            }

            if (slot.expressionIndex != dsInvalidIndex)
            {
                DS_VALIDATE(slot.variableIndex == dsInvalidIndex);
                DS_VALIDATE(slot.expressionIndex.value() < header.expressions.count);
            }

            if (slot.nodeIndex != dsInvalidIndex)
                DS_VALIDATE(slot.nodeIndex.value() < header.nodes.count);
        }

        for (dsAssemblyOutputSlot const& slot : header.outputSlots)
        {
            if (slot.variableIndex != dsInvalidIndex)
                DS_VALIDATE(slot.variableIndex.value() < header.variables.count);
        }

        for (dsAssemblyDependency const& dependency : header.dependencies)
        {
            DS_VALIDATE(dependency.nodeIndex.value() < header.nodes.count);
            DS_VALIDATE(dependency.slotIndex.value() < header.inputSlots.count);
        }

        for (dsAssemblyExpression const& expression : header.expressions)
            DS_VALIDATE(isInRange(expression.codeStart.value(), expression.codeCount, header.byteCode.count));

        return true;
    }

    uint64_t dsHashAssembly(dsAssemblyHeader const* assembly) noexcept
    {
        if (assembly == nullptr)
            return 0;

        if (assembly->size < sizeof(dsAssemblyHeader))
            return 0;

        // we need to hash all bytes of the range _except_ the hash field itself,
        // which we will assume is 0xfe. We'll accomplish this by lazily copying the
        // header, setting hash field to 0, and hashing it. Then hashing in the
        // additional bytes as calculated by the size field minus the size of the
        // header itself.

        dsAssemblyHeader headerCopy = *assembly;
        headerCopy.hash = 0u;

        uint8_t const* const bytes = reinterpret_cast<uint8_t const*>(assembly);

        uint64_t hash = dsHashFnv1a64(bytes, sizeof(headerCopy));
        hash = dsHashFnv1a64(bytes + sizeof(dsAssemblyHeader), assembly->size - sizeof(dsAssemblyHeader));
        return hash;
    }

    static void nullNode(dsNodeContext&, dsEventType, void*) {}
    static dsValue missingFunction(dsFunctionContext& context, void* userData) { return {}; }

    dsAssembly* dsLoadAssembly(dsAllocator& alloc, dsRuntimeHost& host, uint8_t const* bytes, uint32_t size)
    {
        if (!dsValidateAssembly(bytes, size))
            return nullptr;

        dsAssemblyHeader const& header = *reinterpret_cast<dsAssemblyHeader const*>(bytes);

        uint32_t assemblySize = sizeof(dsAssembly);

        uint32_t const headerOffset = dsAlign(assemblySize, alignof(dsAssemblyHeader));
        assemblySize = headerOffset + header.size;

        uint32_t const nodeImplOffset = dsAlign(assemblySize, alignof(dsAssemblyNodeImpl));
        assemblySize = nodeImplOffset + header.nodes.count * sizeof(dsAssemblyNodeImpl);

        uint32_t const constantsOffset = dsAlign(assemblySize, alignof(dsValue));
        assemblySize = constantsOffset + header.constants.count * sizeof(dsValue);

        uint32_t const functionImplOffset = dsAlign(assemblySize, alignof(dsAssemblyFunctionImpl));
        assemblySize = functionImplOffset + header.functions.count * sizeof(dsAssemblyFunctionImpl);

        static_assert(alignof(dsAssemblyHeader) <= alignof(dsAssembly));
        dsAssembly* const assembly = new (alloc.allocate(assemblySize, alignof(dsAssembly))) dsAssembly(alloc, assemblySize);

        std::memcpy(reinterpret_cast<uint8_t*>(assembly) + headerOffset, bytes, header.size);

        assembly->header.assign(reinterpret_cast<uintptr_t>(assembly), headerOffset);
        assembly->nodes.assign(reinterpret_cast<uintptr_t>(assembly), nodeImplOffset, header.nodes.count);
        assembly->constants.assign(reinterpret_cast<uintptr_t>(assembly), constantsOffset, header.constants.count);
        assembly->functions.assign(reinterpret_cast<uintptr_t>(assembly), functionImplOffset, header.functions.count);

        // calculate size and offets for instance data
        assembly->instanceSize = sizeof(dsInstance);

        assembly->instanceStatesOffset = decltype(dsInstance::activeNodes)::allocate(assembly->instanceSize, header.nodes.count);
        assembly->instanceInputPlugsOffset =
            decltype(dsInstance::activeInputPlugs)::allocate(assembly->instanceSize, header.inputPlugCount);
        assembly->instanceOutputPlugsOffset =
            decltype(dsInstance::activeOutputPlugs)::allocate(assembly->instanceSize, header.outputPlugs.count);
        assembly->instanceValuesOffset = decltype(dsInstance::values)::allocate(assembly->instanceSize, header.variables.count);

        // deserialize constants
        for (dsAssemblyConstantIndex constantIndex{0}; constantIndex != header.constants.count; ++constantIndex)
        {
            dsAssemblyConstant const& serialized = header.constants[constantIndex];

            dsValue& deserialized = assembly->constants[constantIndex];
            switch (serialized.type)
            {
            case dsValueType::Nil: deserialized = nullptr; break;
            case dsValueType::Int32: deserialized = dsBitCast<int32_t>(static_cast<uint32_t>(serialized.serialized)); break;
            case dsValueType::Float32: deserialized = dsBitCast<float>(static_cast<uint32_t>(serialized.serialized)); break;
            }
        }

        // fill function implementations
        for (dsAssemblyFunctionIndex functionIndex{0}; functionIndex != header.functions.count; ++functionIndex)
        {
            dsFunctionRuntimeMeta meta;
            if (host.lookupFunction(header.functions[functionIndex], meta) && meta.function != nullptr)
                assembly->functions[functionIndex] = {.function = meta.function, .userData = meta.userData};
            else
                assembly->functions[functionIndex] = {.function = missingFunction};
        }

        // fill node implementations and node userData offsets
        for (dsAssemblyNodeIndex nodeIndex{0}; nodeIndex != header.nodes.count; ++nodeIndex)
        {
            dsNodeRuntimeMeta meta;
            if (host.lookupNode(header.nodes[nodeIndex].typeId, meta) && meta.function != nullptr)
            {
                dsNodeFunction const function = meta.function;

                uint32_t const userOffset = dsAlign(assembly->instanceSize, meta.userAlign);
                assembly->instanceSize = userOffset + meta.userSize;

                assembly->nodes[nodeIndex] = dsAssemblyNodeImpl{.function = function, .userOffset = userOffset};
            }
            else
            {
                assembly->nodes[nodeIndex] = dsAssemblyNodeImpl{.function = nullNode};
            }
        }

        return assembly;
    }

    void dsAcquireAssembly(dsAssembly* assembly) noexcept
    {
        if (assembly != nullptr)
        {
            ++assembly->references;
        }
    }

    void dsReleaseAssembly(dsAssembly* assembly)
    {
        if (assembly != nullptr && --assembly->references == 0)
        {
            dsAllocator* const alloc = &assembly->allocator;
            uint32_t const size = assembly->assemblySize;

            assembly->~dsAssembly();

            alloc->free(assembly, size, alignof(dsAssembly));
        }
    }
} // namespace descript
