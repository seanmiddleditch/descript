// descript

#include "descript/graph_compiler.hh"

#include "descript/alloc.hh"
#include "descript/assembly.hh"
#include "descript/expression_compiler.hh"
#include "descript/value.hh"

#include "array.hh"
#include "assembly_internal.hh"
#include "assert.hh"
#include "bit.hh"
#include "fnv.hh"
#include "index.hh"
#include "ops.hh"
#include "string.hh"

#include <new>

namespace descript {
    namespace {
        class GraphCompiler final : public dsGraphCompiler
        {
        public:
            explicit GraphCompiler(dsAllocator& alloc, dsGraphCompilerHost& host) noexcept
                : allocator_(alloc), host_(host), entries_(alloc), nodes_(alloc), inputPlugs_(alloc), outputPlugs_(alloc), wires_(alloc),
                  inputSlots_(alloc), outputSlots_(alloc), variables_(alloc), dependencies_(alloc), plugWireLinks_(alloc),
                  inputBindings_(alloc), outputBindings_(alloc), expressions_(alloc), constants_(alloc), functions_(alloc),
                  byteCode_(alloc), errors_(alloc), assemblyBytes_(alloc), graphName_(alloc), debugName_(alloc)
            {
            }
            ~GraphCompiler() { dsDestroyExpressionCompiler(exprCompiler_); }

            void reset() override;

            void setGraphName(char const* name, char const* nameEnd = nullptr) override;
            void setDebugName(char const* name, char const* nameEnd = nullptr) override;

            void beginNode(dsNodeId nodeId, dsNodeTypeId nodeTypeId) override;
            void beginInputSlot(dsInputSlot slot, dsTypeId type) override;
            void beginOutputSlot(dsOutputSlot slot, dsTypeId type) override;

            void addInputPlug(dsInputPlugIndex inputPlugIndex) override;
            void addOutputPlug(dsOutputPlugIndex outputPlugIndex) override;

            void addWire(dsNodeId fromNodeId, dsOutputPlugIndex fromPlugIndex, dsNodeId toNodeId, dsInputPlugIndex toPlugIndex) override;

            void addVariable(dsTypeId type, char const* name, char const* nameEnd) override;

            void bindVariable(char const* name, char const* nameEnd = nullptr) override;
            void bindExpression(char const* expression, char const* expressionEnd = nullptr) override;
            void bindConstant(dsValueRef const& value) override;

            bool compile() override;
            bool build() override;

            uint32_t getErrorCount() const noexcept override;
            dsCompileError getError(uint32_t index) const noexcept override;

            uint8_t const* assemblyBytes() const noexcept override { return assemblyBytes_.data(); };
            uint32_t assemblySize() const noexcept override { return assemblyBytes_.size(); }

            dsAllocator& allocator() noexcept { return allocator_; }

        private:
            DS_DEFINE_INDEX(NodeIndex);
            DS_DEFINE_INDEX(InputPlugIndex);
            DS_DEFINE_INDEX(OutputPlugIndex);
            DS_DEFINE_INDEX(InputSlotIndex);
            DS_DEFINE_INDEX(OutputSlotIndex);
            DS_DEFINE_INDEX(PlugWireLinkIndex);
            DS_DEFINE_INDEX(DependencyIndex);
            DS_DEFINE_INDEX(VariableIndex);
            DS_DEFINE_INDEX(WireIndex);
            DS_DEFINE_INDEX(InputBindingIndex);
            DS_DEFINE_INDEX(OutputBindingIndex);
            DS_DEFINE_INDEX(ExpressionIndex);

            class ExpressionCompilerHost;
            class ExpressionBuilder;

            enum class CompileStatus
            {
                Reset,
                Compiled,
                Errored,
            };

            struct Node
            {
                // source data
                dsNodeId nodeId;
                dsNodeTypeId typeId;

                // cached data
                dsNodeKind kind = dsNodeKind::State;

                // plug and slot lists
                OutputPlugIndex firstOutputPlug = dsInvalidIndex;
                InputPlugIndex firstInputPlug = dsInvalidIndex;
                InputSlotIndex firstInputSlot = dsInvalidIndex;
                OutputSlotIndex firstOutputSlot = dsInvalidIndex;

                // compiled data
                dsAssemblyNodeIndex index = dsInvalidIndex;
                dsAssemblyOutputPlugIndex outputPlugStart = dsInvalidIndex;
                uint32_t inputPlugCount = 0;
                uint32_t outputPlugCount = 0;
                dsAssemblyInputSlotIndex inputSlotStart = dsInvalidIndex;
                dsAssemblyOutputSlotIndex outputSlotStart = dsInvalidIndex;
                uint32_t inputSlotCount = 0;
                uint32_t outputSlotCount = 0;
                InputPlugIndex beginPlugIndex = dsInvalidIndex;
                OutputPlugIndex outputPlugIndex = dsInvalidIndex;
                bool live = false;
            };

            struct InputSlot
            {
                // source data
                dsNodeId nodeId;
                dsInputSlot inputSlot;
                dsTypeId type;

                // slot list
                InputSlotIndex nextSlot = dsInvalidIndex;

                // compiled data
                NodeIndex nodeIndex = dsInvalidIndex;
                InputBindingIndex bindingIndex = dsInvalidIndex;
                dsAssemblyInputSlotIndex index = dsInvalidIndex;
                bool live = false;
            };

            struct OutputSlot
            {
                // source data
                dsNodeId nodeId;
                dsOutputSlot outputSlot;
                dsTypeId type;

                // slot list
                OutputSlotIndex nextSlot = dsInvalidIndex;

                // compiled data
                NodeIndex nodeIndex = dsInvalidIndex;
                OutputBindingIndex bindingIndex = dsInvalidIndex;
                dsAssemblyOutputSlotIndex index = dsInvalidIndex;
                bool live = false;
            };

            struct InputPlug
            {
                // source data
                dsNodeId nodeId;
                dsInputPlugIndex inputPlugIndex;

                // plug list
                InputPlugIndex nextPlug = dsInvalidIndex;

                // compiled data
                NodeIndex nodeIndex = dsInvalidIndex;
                bool live = false;
            };

            struct OutputPlug
            {
                // source data
                dsNodeId nodeId;
                dsOutputPlugIndex outputPlugIndex;

                // plug list
                OutputPlugIndex nextPlug = dsInvalidIndex;

                // wire link list
                PlugWireLinkIndex firstLink = dsInvalidIndex;

                // compiled data
                NodeIndex nodeIndex = dsInvalidIndex;
                dsAssemblyOutputPlugIndex index = dsInvalidIndex;
                dsAssemblyWireIndex wireStart = dsInvalidIndex;
                uint32_t wireCount = 0;
                bool live = false;
            };

            struct Wire
            {
                // source data
                dsNodeId fromNodeId;
                dsNodeId toNodeId;
                dsOutputPlugIndex fromPlugIndex;
                dsInputPlugIndex toPlugIndex;

                // compiled data
                OutputPlugIndex outputPlugIndex = dsInvalidIndex;
                InputPlugIndex inputPlugIndex = dsInvalidIndex;
                dsAssemblyWireIndex index = dsInvalidIndex;
                bool live = false;
            };

            struct Variable
            {
                // source data
                dsString name;
                uint64_t nameHash = 0;
                dsTypeId type;

                // compiled data
                dsAssemblyVariableIndex index = dsInvalidIndex;
                dsAssemblyDependencyIndex dependencyStart = dsInvalidIndex;
                DependencyIndex firstDependency = dsInvalidIndex;
                uint32_t dependencyCount = 0;
                bool live = false;
            };

            struct PlugWireLink
            {
                WireIndex wireIndex = dsInvalidIndex;
                PlugWireLinkIndex nextLink = dsInvalidIndex;
            };

            struct Dependency
            {
                InputSlotIndex slotIndex = dsInvalidIndex;
                dsAssemblyDependencyIndex index = dsInvalidIndex;
                DependencyIndex nextDependency = dsInvalidIndex;
            };

            struct InputBinding
            {
                // source data
                InputSlotIndex slotIndex = dsInvalidIndex;

                // source data - only one of these should be set
                dsString variableName;
                ExpressionIndex expressionIndex = dsInvalidIndex;
                dsAssemblyConstantIndex constantIndex = dsInvalidIndex;

                // compiled data
                VariableIndex variableIndex = dsInvalidIndex;
                bool live = false;
            };

            struct OutputBinding
            {
                // source data
                OutputSlotIndex slotIndex = dsInvalidIndex;
                dsString variableName;

                // compiled data
                VariableIndex variableIndex = dsInvalidIndex;
                bool live = false;
            };

            struct Expression
            {
                // source data
                dsString expression;

                // compiled data
                dsAssemblyExpressionIndex index = dsInvalidIndex;
                dsAssemblyByteCodeIndex byteCodeStart = dsInvalidIndex;
                uint32_t byteCodeCount = 0;
                bool live = false;
            };

            void resolveNodes();
            void linkElements();
            void findEntries();
            void updateLiveness();
            void traceLiveness(NodeIndex nodeIndex);
            void processPlugs();
            void processWires();
            void compileBindings();
            void allocateIndices();

            // return false, for convenience
            bool error(dsCompileError const& error);

            NodeIndex findNode(dsNodeId nodeId) const noexcept;
            InputPlugIndex findPlug(dsNodeId nodeId, dsInputPlugIndex plugIndex) const noexcept;
            OutputPlugIndex findPlug(dsNodeId nodeId, dsOutputPlugIndex plugIndex) const noexcept;

            dsAllocator& allocator_;
            dsGraphCompilerHost& host_;
            dsExpressionCompiler* exprCompiler_ = nullptr;
            dsArray<NodeIndex> entries_;
            dsArray<Node, NodeIndex> nodes_;
            dsArray<InputPlug, InputPlugIndex> inputPlugs_;
            dsArray<OutputPlug, OutputPlugIndex> outputPlugs_;
            dsArray<Wire, WireIndex> wires_;
            dsArray<InputSlot, InputSlotIndex> inputSlots_;
            dsArray<OutputSlot, OutputSlotIndex> outputSlots_;
            dsArray<Variable, VariableIndex> variables_;
            dsArray<Dependency, DependencyIndex> dependencies_;
            dsArray<PlugWireLink, PlugWireLinkIndex> plugWireLinks_;
            dsArray<InputBinding, InputBindingIndex> inputBindings_;
            dsArray<OutputBinding, OutputBindingIndex> outputBindings_;
            dsArray<Expression, ExpressionIndex> expressions_;
            dsArray<dsValueStorage, dsAssemblyConstantIndex> constants_;
            dsArray<dsFunctionId, dsAssemblyFunctionIndex> functions_;
            dsArray<uint8_t, dsAssemblyByteCodeIndex> byteCode_;
            dsArray<dsCompileError> errors_;
            dsArray<uint8_t> assemblyBytes_;
            dsString graphName_;
            dsString debugName_;
            uint32_t compiledNodeCount_ = 0;
            uint32_t compiledInputPlugCount_ = 0;
            uint32_t compiledOutputPlugCount_ = 0;
            uint32_t compiledWireCount_ = 0;
            uint32_t compiledInputSlotCount_ = 0;
            uint32_t compiledOutputSlotCount_ = 0;
            uint32_t compiledVariableCount_ = 0;
            uint32_t compiledDependencyCount_ = 0;
            uint32_t compiledExpressionCount_ = 0;
            NodeIndex openNode_ = dsInvalidIndex;
            InputSlotIndex openInputSlot_ = dsInvalidIndex;
            OutputSlotIndex openOutputSlot_ = dsInvalidIndex;
            CompileStatus status_ = CompileStatus::Reset;
        };
    } // namespace

    dsGraphCompiler* dsCreateGraphCompiler(dsAllocator& alloc, dsGraphCompilerHost& host)
    {
        return new (alloc.allocate(sizeof(GraphCompiler), alignof(GraphCompiler))) GraphCompiler(alloc, host);
    }

    void dsDestroyGraphCompiler(dsGraphCompiler* compiler)
    {
        if (compiler != nullptr)
        {
            GraphCompiler* impl = static_cast<GraphCompiler*>(compiler);
            dsAllocator& alloc = impl->allocator();
            impl->~GraphCompiler();
            alloc.free(impl, sizeof(GraphCompiler), alignof(GraphCompiler));
        }
    }

    void GraphCompiler::setGraphName(char const* name, char const* nameEnd)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        graphName_.reset(name, nameEnd);
    }

    void GraphCompiler::setDebugName(char const* name, char const* nameEnd)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        debugName_.reset(name, nameEnd);
    }

    void GraphCompiler::beginNode(dsNodeId nodeId, dsNodeTypeId nodeTypeId)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);

        openInputSlot_ = dsInvalidIndex;
        openOutputSlot_ = dsInvalidIndex;

        for (auto&& [index, node] : dsEnumerate(nodes_))
        {
            if (node.nodeId == nodeId)
            {
                openNode_ = NodeIndex{index};
                node.typeId = nodeTypeId;
                return;
            }
        }

        openNode_ = NodeIndex{nodes_.size()};
        nodes_.pushBack(Node{.nodeId = nodeId, .typeId = nodeTypeId});
    }

    void GraphCompiler::beginInputSlot(dsInputSlot slot, dsTypeId type)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(openNode_ != dsInvalidIndex);

        openOutputSlot_ = dsInvalidIndex;

        for (auto&& [index, inputSlot] : dsEnumerate(inputSlots_))
        {
            if (inputSlot.nodeIndex == openNode_ && inputSlot.inputSlot == slot)
            {
                openInputSlot_ = InputSlotIndex{index};
                inputSlot.type = type;
                return;
            }
        }

        openInputSlot_ = InputSlotIndex{inputSlots_.size()};
        inputSlots_.pushBack(InputSlot{.nodeId = nodes_[openNode_].nodeId, .inputSlot = slot, .type = type, .nodeIndex = openNode_});
    }

    void GraphCompiler::beginOutputSlot(dsOutputSlot slot, dsTypeId type)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(openNode_ != dsInvalidIndex);

        openInputSlot_ = dsInvalidIndex;

        for (auto&& [index, inputSlot] : dsEnumerate(outputSlots_))
        {
            if (inputSlot.nodeIndex == openNode_ && inputSlot.outputSlot == slot)
            {
                openOutputSlot_ = OutputSlotIndex{index};
                inputSlot.type = type;
                return;
            }
        }

        openOutputSlot_ = OutputSlotIndex{outputSlots_.size()};
        outputSlots_.pushBack(OutputSlot{.nodeId = nodes_[openNode_].nodeId, .outputSlot = slot, .type = type, .nodeIndex = openNode_});
    }

    void GraphCompiler::addInputPlug(dsInputPlugIndex inputPlugIndex)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(openNode_ != dsInvalidIndex);

        openInputSlot_ = dsInvalidIndex;
        openOutputSlot_ = dsInvalidIndex;

        inputPlugs_.pushBack(InputPlug{.nodeId = nodes_[openNode_].nodeId, .inputPlugIndex = inputPlugIndex, .nodeIndex = openNode_});
    }

    void GraphCompiler::addOutputPlug(dsOutputPlugIndex outputPlugIndex)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(openNode_ != dsInvalidIndex);

        openInputSlot_ = dsInvalidIndex;
        openOutputSlot_ = dsInvalidIndex;

        outputPlugs_.pushBack(OutputPlug{.nodeId = nodes_[openNode_].nodeId, .outputPlugIndex = outputPlugIndex, .nodeIndex = openNode_});
    }

    void GraphCompiler::addWire(dsNodeId fromNodeId, dsOutputPlugIndex fromPlugIndex, dsNodeId toNodeId, dsInputPlugIndex toPlugIndex)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);

        openNode_ = dsInvalidIndex;
        openInputSlot_ = dsInvalidIndex;
        openOutputSlot_ = dsInvalidIndex;

        wires_.pushBack(Wire{.fromNodeId = fromNodeId, .toNodeId = toNodeId, .fromPlugIndex = fromPlugIndex, .toPlugIndex = toPlugIndex});
    }

    void GraphCompiler::addVariable(dsTypeId type, char const* name, char const* nameEnd)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(!dsIsEmpty(name, nameEnd));

        openNode_ = dsInvalidIndex;
        openInputSlot_ = dsInvalidIndex;
        openOutputSlot_ = dsInvalidIndex;

        uint64_t const nameHash = dsHashFnv1a64(name, nameEnd);

        variables_.pushBack(Variable{.name = dsString(allocator_, name, nameEnd), .nameHash = nameHash, .type = type});
    }

    void GraphCompiler::bindVariable(char const* name, char const* nameEnd)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(openNode_ != dsInvalidIndex);
        DS_GUARD_VOID(openInputSlot_ != dsInvalidIndex || openOutputSlot_ != dsInvalidIndex);
        DS_GUARD_VOID(!dsIsEmpty(name, nameEnd));

        if (openInputSlot_ != dsInvalidIndex)
        {
            inputBindings_.pushBack(InputBinding{
                .slotIndex = openInputSlot_,
                .variableName = dsString(allocator_, name, nameEnd),
            });
        }
        else
        {
            outputBindings_.pushBack(OutputBinding{
                .slotIndex = openOutputSlot_,
                .variableName = dsString(allocator_, name, nameEnd),
            });
        }
    }

    void GraphCompiler::bindExpression(char const* expression, char const* expressionEnd)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(openNode_ != dsInvalidIndex);
        DS_GUARD_VOID(openInputSlot_ != dsInvalidIndex);

        ExpressionIndex const exprIndex{expressions_.size()};
        expressions_.pushBack(Expression{.expression = dsString(allocator_, expression, expressionEnd)});
        inputBindings_.pushBack(InputBinding{
            .slotIndex = openInputSlot_,
            .variableName = dsString(allocator_),
            .expressionIndex = exprIndex,
        });
    }

    void GraphCompiler::bindConstant(dsValueRef const& value)
    {
        DS_GUARD_VOID(status_ == CompileStatus::Reset);
        DS_GUARD_VOID(openNode_ != dsInvalidIndex);
        DS_GUARD_VOID(openInputSlot_ != dsInvalidIndex);

        // FIXME: use a separate table for constant bindings, track liveness,
        // only emit them if the slot is in use
        for (auto&& [constantIndex, constant] : dsEnumerate(constants_))
        {
            if (constant == value)
            {
                inputBindings_.pushBack(InputBinding{
                    .slotIndex = openInputSlot_,
                    .variableName = dsString(allocator_),
                    .constantIndex = dsAssemblyConstantIndex{constantIndex},
                });
                return;
            }
        }

        dsAssemblyConstantIndex const constantIndex{constants_.size()};
        constants_.pushBack(value);
        inputBindings_.pushBack(InputBinding{
            .slotIndex = openInputSlot_,
            .variableName = dsString(allocator_),
            .constantIndex = constantIndex,
        });
    }

    bool GraphCompiler::compile()
    {
        DS_GUARD_OR(status_ == CompileStatus::Reset, false);

        openNode_ = dsInvalidIndex;
        openInputSlot_ = dsInvalidIndex;
        openOutputSlot_ = dsInvalidIndex;

        resolveNodes();
        linkElements();
        findEntries();
        processPlugs();
        processWires();
        updateLiveness();
        compileBindings();
        allocateIndices();

        bool const success = errors_.empty();
        status_ = success ? CompileStatus::Compiled : CompileStatus::Errored;
        return success;
    }

    void GraphCompiler::reset()
    {
        status_ = CompileStatus::Reset;

        entries_.clear();
        nodes_.clear();
        outputPlugs_.clear();
        wires_.clear();
        inputSlots_.clear();
        outputSlots_.clear();
        variables_.clear();
        dependencies_.clear();
        inputBindings_.clear();
        outputBindings_.clear();
        dependencies_.clear();
        expressions_.clear();
        constants_.clear();
        // functions_.clear();
        byteCode_.clear();
        errors_.clear();
        assemblyBytes_.clear();
        graphName_.reset();
        debugName_.reset();
        compiledNodeCount_ = 0;
        compiledInputPlugCount_ = 0;
        compiledOutputPlugCount_ = 0;
        compiledWireCount_ = 0;
        compiledInputSlotCount_ = 0;
        compiledOutputSlotCount_ = 0;
        compiledVariableCount_ = 0;
        compiledDependencyCount_ = 0;
        openNode_ = dsInvalidIndex;
        openInputSlot_ = dsInvalidIndex;
        openOutputSlot_ = dsInvalidIndex;
    }

    bool GraphCompiler::build()
    {
        DS_GUARD_OR(status_ == CompileStatus::Compiled, false);

        uint32_t offset = sizeof(dsAssemblyHeader);
        uint32_t const nodesOffset = dsAlign(offset, alignof(dsAssemblyNode));
        offset = nodesOffset + compiledNodeCount_ * sizeof(dsAssemblyNode);

        uint32_t const entryNodesOffset = dsAlign(offset, alignof(dsAssemblyNodeIndex));
        offset = entryNodesOffset + entries_.size() * sizeof(dsAssemblyNodeIndex);

        uint32_t const plugsOffset = dsAlign(offset, alignof(dsAssemblyOutputPlug));
        offset = plugsOffset + compiledOutputPlugCount_ * sizeof(dsAssemblyOutputPlug);

        uint32_t const wiresOffset = dsAlign(offset, alignof(dsAssemblyWire));
        offset = wiresOffset + compiledWireCount_ * sizeof(dsAssemblyWire);

        uint32_t const inputSlotsOffset = dsAlign(offset, alignof(dsAssemblyInputSlot));
        offset = inputSlotsOffset + compiledInputSlotCount_ * sizeof(dsAssemblyInputSlot);

        uint32_t const outputSlotsOffset = dsAlign(offset, alignof(dsAssemblyOutputSlot));
        offset = outputSlotsOffset + compiledOutputSlotCount_ * sizeof(dsAssemblyOutputSlot);

        uint32_t const varsOffset = dsAlign(offset, alignof(dsAssemblyVariable));
        offset = varsOffset + compiledVariableCount_ * sizeof(dsAssemblyVariable);

        uint32_t const depsOffset = dsAlign(offset, alignof(dsAssemblyDependency));
        offset = depsOffset + compiledDependencyCount_ * sizeof(dsAssemblyDependency);

        uint32_t const expressionsOffset = dsAlign(offset, alignof(dsAssemblyExpression));
        offset = expressionsOffset + compiledExpressionCount_ * sizeof(dsAssemblyExpression);

        uint32_t const functionsOffset = dsAlign(offset, alignof(dsFunctionId));
        offset = functionsOffset + functions_.size() * sizeof(dsFunctionId);

        uint32_t const constantsOffset = dsAlign(offset, alignof(dsAssemblyConstant));
        offset = constantsOffset + constants_.size() * sizeof(dsAssemblyConstant);

        uint32_t const byteCodeOffset = dsAlign(offset, alignof(uint8_t));
        offset = byteCodeOffset + byteCode_.size() * sizeof(uint8_t);

        uint32_t const size = offset;

        assemblyBytes_.resize(size);
        std::memset(assemblyBytes_.data(), 0xfe, assemblyBytes_.size());
        dsAssemblyHeader* const header = reinterpret_cast<dsAssemblyHeader*>(assemblyBytes_.data());

        header->version = 0;
        header->size = size;
        header->hash = 0;
        header->inputPlugCount = compiledInputPlugCount_;
        header->nodes.assign(reinterpret_cast<uintptr_t>(header), nodesOffset, compiledNodeCount_);
        header->entryNodes.assign(reinterpret_cast<uintptr_t>(header), entryNodesOffset, entries_.size());
        header->outputPlugs.assign(reinterpret_cast<uintptr_t>(header), plugsOffset, compiledOutputPlugCount_);
        header->wires.assign(reinterpret_cast<uintptr_t>(header), wiresOffset, compiledWireCount_);
        header->inputSlots.assign(reinterpret_cast<uintptr_t>(header), inputSlotsOffset, compiledInputSlotCount_);
        header->outputSlots.assign(reinterpret_cast<uintptr_t>(header), outputSlotsOffset, compiledOutputSlotCount_);
        header->variables.assign(reinterpret_cast<uintptr_t>(header), varsOffset, compiledVariableCount_);
        header->dependencies.assign(reinterpret_cast<uintptr_t>(header), depsOffset, compiledDependencyCount_);
        header->expressions.assign(reinterpret_cast<uintptr_t>(header), expressionsOffset, compiledExpressionCount_);
        header->functions.assign(reinterpret_cast<uintptr_t>(header), functionsOffset, functions_.size());
        header->constants.assign(reinterpret_cast<uintptr_t>(header), constantsOffset, constants_.size());
        header->byteCode.assign(reinterpret_cast<uintptr_t>(header), byteCodeOffset, byteCode_.size());

        for (auto const&& [index, node] : dsEnumerate(nodes_))
        {
            if (!node.live)
                continue;

            dsAssemblyNode& outNode = header->nodes[node.index];
            outNode.typeId = node.typeId;
            outNode.outputPlug = node.outputPlugIndex != dsInvalidIndex && outputPlugs_[node.outputPlugIndex].index != dsInvalidIndex
                                     ? outputPlugs_[node.outputPlugIndex].index
                                     : dsInvalidIndex;
            outNode.customOutputPlugStart = node.outputPlugStart;
            outNode.customOutputPlugCount = node.outputPlugCount;
            outNode.customInputPlugCount = node.inputPlugCount;
            outNode.inputSlotStart = node.inputSlotStart;
            outNode.inputSlotCount = node.inputSlotCount;
            outNode.outputSlotStart = node.outputSlotStart;
            outNode.outputSlotCount = node.outputSlotCount;
        }

        for (uint32_t index = 0; index != entries_.size(); ++index)
            header->entryNodes[index] = nodes_[entries_[index]].index;

        // reset all plugs, since not all are written but should not contain invalid data
        for (dsAssemblyOutputPlug& outputPlug : header->outputPlugs)
        {
            outputPlug.wireStart = dsInvalidIndex;
            outputPlug.wireCount = 0;
        }

        for (OutputPlug const& outputPlug : outputPlugs_)
        {
            if (!outputPlug.live)
                continue;

            dsAssemblyOutputPlug& outOutputPlug = header->outputPlugs[outputPlug.index];
            outOutputPlug.wireStart = outputPlug.wireStart;
            outOutputPlug.wireCount = outputPlug.wireCount;
        }

        for (Wire const& wire : wires_)
        {
            if (!wire.live)
                continue;

            dsAssemblyWire& outWire = header->wires[wire.index];
            InputPlug const& toPlug = inputPlugs_[wire.inputPlugIndex];
            outWire.nodeIndex = nodes_[toPlug.nodeIndex].index;
            outWire.inputPlugIndex = toPlug.inputPlugIndex;
        }

        for (dsAssemblyInputSlot& slot : header->inputSlots)
        {
            slot.variableIndex = dsInvalidIndex;
            slot.expressionIndex = dsInvalidIndex;
            slot.constantIndex = dsInvalidIndex;
            slot.nodeIndex = dsInvalidIndex;
        }

        for (dsAssemblyOutputSlot& slot : header->outputSlots)
            slot.variableIndex = dsInvalidIndex;

        for (InputSlot const& slot : inputSlots_)
        {
            if (!slot.live)
                continue;

            dsAssemblyInputSlot& outSlot = header->inputSlots[slot.index];
            outSlot.variableIndex = dsInvalidIndex;
            outSlot.expressionIndex = dsInvalidIndex;
            outSlot.nodeIndex = nodes_[slot.nodeIndex].index;

            InputBinding const& binding = inputBindings_[slot.bindingIndex];
            if (binding.variableIndex != dsInvalidIndex)
                outSlot.variableIndex = variables_[binding.variableIndex].index;
            else if (binding.expressionIndex != dsInvalidIndex)
                outSlot.expressionIndex = expressions_[binding.expressionIndex].index;
            else if (binding.constantIndex != dsInvalidIndex)
                outSlot.constantIndex = binding.constantIndex;
        }

        for (OutputSlot const& slot : outputSlots_)
        {
            if (!slot.live)
                continue;

            dsAssemblyOutputSlot& outSlot = header->outputSlots[slot.index];
            outSlot.variableIndex = dsInvalidIndex;

            OutputBinding const& binding = outputBindings_[slot.bindingIndex];
            if (binding.variableIndex != dsInvalidIndex)
                outSlot.variableIndex = variables_[binding.variableIndex].index;
        }

        for (Variable const& var : variables_)
        {
            if (!var.live)
                continue;

            dsAssemblyVariable& outVar = header->variables[var.index];
            outVar.nameHash = var.nameHash;
            outVar.dependencyStart = var.dependencyStart;
            outVar.dependencyCount = var.dependencyCount;
        }

        for (Dependency const& dep : dependencies_)
        {
            dsAssemblyDependency& outDep = header->dependencies[dep.index];
            outDep.nodeIndex = nodes_[inputSlots_[dep.slotIndex].nodeIndex].index;
            outDep.slotIndex = inputSlots_[dep.slotIndex].index;
        }

        for (Expression const& expression : expressions_)
        {
            if (!expression.live)
                continue;

            dsAssemblyExpression& outExpr = header->expressions[expression.index];
            outExpr.codeStart = expression.byteCodeStart;
            outExpr.codeCount = expression.byteCodeCount;
        }

        for (auto&& [index, value] : dsEnumerate(constants_))
        {
            dsAssemblyConstant& outConst = header->constants[dsAssemblyConstantIndex{index}];
            outConst.typeId = value.type().id();
            outConst.serialized = 0;

            if (value.type() == dsType<int32_t>)
                outConst.serialized = dsBitCast<uint32_t>(value.as<int32_t>());
            else if (value.type() == dsType<float>)
                outConst.serialized = dsBitCast<uint32_t>(value.as<float>());
            else if (value.type() != dsType<decltype(nullptr)>)
                DS_GUARD_OR(false, false, "Unsupported value type");
        }

        for (auto&& [index, functionId] : dsEnumerate(functions_))
            header->functions[dsAssemblyFunctionIndex{index}] = functionId;

        std::memcpy(header->byteCode.data(), byteCode_.data(), byteCode_.size());

        header->hash = dsHashAssembly(reinterpret_cast<dsAssemblyHeader const*>(header));

        DS_ASSERT(dsValidateAssembly(assemblyBytes_.data(), assemblyBytes_.size()));

        return true;
    }

    void GraphCompiler::resolveNodes()
    {
        for (Node& node : nodes_)
        {
            dsNodeCompileMeta meta;
            if (!host_.lookupNodeType(node.typeId, meta))
            {
                error({.code = dsCompileErrorCode::UnknownNodeType}); // FIXME: location?
                continue;
            }

            node.kind = meta.kind;
        }
    }

    void GraphCompiler::linkElements()
    {
        for (auto&& [index, plug] : dsEnumerate(inputPlugs_))
        {
            plug.nodeIndex = findNode(plug.nodeId);
            if (!nodes_.contains(plug.nodeIndex))
            {
                error({.code = dsCompileErrorCode::NodeNotFound}); // FIXME: location?
                continue;
            }

            plug.nextPlug = nodes_[plug.nodeIndex].firstInputPlug;
            nodes_[plug.nodeIndex].firstInputPlug = InputPlugIndex{index};
        }

        for (auto&& [index, plug] : dsEnumerate(outputPlugs_))
        {
            plug.nodeIndex = findNode(plug.nodeId);
            if (!nodes_.contains(plug.nodeIndex))
            {
                error({.code = dsCompileErrorCode::NodeNotFound}); // FIXME: location?
                continue;
            }

            plug.nextPlug = nodes_[plug.nodeIndex].firstOutputPlug;
            nodes_[plug.nodeIndex].firstOutputPlug = OutputPlugIndex{index};
        }

        for (auto&& [index, slot] : dsEnumerate(inputSlots_))
        {
            slot.nodeIndex = findNode(slot.nodeId);
            if (!nodes_.contains(slot.nodeIndex))
            {
                error({.code = dsCompileErrorCode::NodeNotFound}); // FIXME: location?
                continue;
            }

            // add to node's linked list of slots
            slot.nextSlot = nodes_[slot.nodeIndex].firstInputSlot;
            nodes_[slot.nodeIndex].firstInputSlot = InputSlotIndex{index};
        }

        for (auto&& [index, slot] : dsEnumerate(outputSlots_))
        {
            slot.nodeIndex = findNode(slot.nodeId);
            if (!nodes_.contains(slot.nodeIndex))
            {
                error({.code = dsCompileErrorCode::NodeNotFound}); // FIXME: location?
                continue;
            }

            // add to node's linked list of slots
            slot.nextSlot = nodes_[slot.nodeIndex].firstOutputSlot;
            nodes_[slot.nodeIndex].firstOutputSlot = OutputSlotIndex{index};
        }

        for (auto&& [index, wire] : dsEnumerate(wires_))
        {
            wire.outputPlugIndex = findPlug(wire.fromNodeId, wire.fromPlugIndex);
            if (!outputPlugs_.contains(wire.outputPlugIndex))
            {
                error({.code = dsCompileErrorCode::PlugNotFound}); // FIXME: location?
                continue;
            }

            wire.inputPlugIndex = findPlug(wire.toNodeId, wire.toPlugIndex);
            if (!inputPlugs_.contains(wire.inputPlugIndex))
            {
                error({.code = dsCompileErrorCode::PlugNotFound}); // FIXME: location?
                continue;
            }

            // local declaration required to avoid a spurious compile error on MSVC 17.2 preview
            WireIndex const wireIndex{index};

            plugWireLinks_.pushBack(PlugWireLink{.wireIndex = wireIndex, .nextLink = outputPlugs_[wire.outputPlugIndex].firstLink});
            outputPlugs_[wire.outputPlugIndex].firstLink = PlugWireLinkIndex{plugWireLinks_.size() - 1};
        }

        for (auto&& [index, binding] : dsEnumerate(inputBindings_))
        {
            InputSlot& slot = inputSlots_[binding.slotIndex];
            slot.bindingIndex = InputBindingIndex{index};

            if (binding.variableName.empty())
                continue;

            uint64_t const nameHash = dsHashFnv1a64(binding.variableName.cStr());

            for (auto&& [varIndex, variable] : dsEnumerate(variables_))
            {
                if (variable.nameHash == nameHash)
                {
                    binding.variableIndex = VariableIndex{varIndex};
                    break;
                }
            }
        }

        for (auto&& [index, binding] : dsEnumerate(outputBindings_))
        {
            OutputSlot& slot = outputSlots_[binding.slotIndex];
            slot.bindingIndex = OutputBindingIndex{index};

            if (binding.variableName.empty())
                continue;

            uint64_t const nameHash = dsHashFnv1a64(binding.variableName.cStr());

            for (auto&& [varIndex, variable] : dsEnumerate(variables_))
            {
                if (variable.nameHash == nameHash)
                {
                    binding.variableIndex = VariableIndex{varIndex};
                    break;
                }
            }
        }
    }

    uint32_t GraphCompiler::getErrorCount() const noexcept { return errors_.size(); }

    dsCompileError GraphCompiler::getError(uint32_t index) const noexcept
    {
        DS_GUARD_OR(index < errors_.size(), dsCompileError{});
        return errors_[index];
    }

    void GraphCompiler::findEntries()
    {
        for (auto&& [index, node] : dsEnumerate(nodes_))
            if (node.kind == dsNodeKind::Entry)
                entries_.pushBack(NodeIndex{index});

        if (entries_.empty())
            error({.code = dsCompileErrorCode::NoEntries});
    }

    void GraphCompiler::updateLiveness()
    {
        for (NodeIndex const entryNodeIndex : entries_)
        {
            traceLiveness(entryNodeIndex);
        }
    }

    void GraphCompiler::traceLiveness(NodeIndex nodeIndex)
    {
        Node& node = nodes_[nodeIndex];

        // terminate recursion if the node has already been visited
        if (node.live)
        {
            return;
        }

        node.live = true;

        // collect all slots for the node
        for (InputSlotIndex slotIndex = node.firstInputSlot; inputSlots_.contains(slotIndex); slotIndex = inputSlots_[slotIndex].nextSlot)
        {
            InputSlot& slot = inputSlots_[slotIndex];
            if (slot.bindingIndex != dsInvalidIndex)
            {
                InputBinding& binding = inputBindings_[slot.bindingIndex];
                binding.live = true;
            }
        }
        for (OutputSlotIndex slotIndex = node.firstOutputSlot; outputSlots_.contains(slotIndex);
             slotIndex = outputSlots_[slotIndex].nextSlot)
        {
            OutputSlot& slot = outputSlots_[slotIndex];
            if (slot.bindingIndex != dsInvalidIndex)
            {
                OutputBinding& binding = outputBindings_[slot.bindingIndex];
                binding.live = true;
            }
        }

        // collect all outgoing wires from the node
        for (OutputPlugIndex outputPlugIndex = node.firstOutputPlug; outputPlugs_.contains(outputPlugIndex);
             outputPlugIndex = outputPlugs_[outputPlugIndex].nextPlug)
        {
            OutputPlug const& plug = outputPlugs_[outputPlugIndex];
            for (PlugWireLinkIndex linkIndex = plug.firstLink; plugWireLinks_.contains(linkIndex);
                 linkIndex = plugWireLinks_[linkIndex].nextLink)
            {
                WireIndex const wireIndex = plugWireLinks_[linkIndex].wireIndex;
                Wire const& wire = wires_[wireIndex];

                // mark liveness for both plugs and the wire
                outputPlugs_[wire.outputPlugIndex].live = true;
                inputPlugs_[wire.inputPlugIndex].live = true;
                wires_[wireIndex].live = true;

                traceLiveness(inputPlugs_[wire.inputPlugIndex].nodeIndex);
            }
        }
    }

    void GraphCompiler::processPlugs()
    {
        for (auto&& [index, plug] : dsEnumerate(inputPlugs_))
        {
            Node& node = nodes_[plug.nodeIndex];

            // assign special plug indices
            if (plug.inputPlugIndex == dsBeginPlugIndex)
            {
                if (node.beginPlugIndex != dsInvalidIndex)
                    error(dsCompileError{.code = dsCompileErrorCode::DuplicateBuiltinPlug}); // FIXME: location?
                else
                    node.beginPlugIndex = InputPlugIndex{index};
            }
        }

        for (auto&& [index, plug] : dsEnumerate(outputPlugs_))
        {
            Node& node = nodes_[plug.nodeIndex];

            // assign special plug indices
            if (plug.outputPlugIndex == dsDefaultOutputPlugIndex)
            {
                if (node.outputPlugIndex != dsInvalidIndex)
                    error(dsCompileError{.code = dsCompileErrorCode::DuplicateBuiltinPlug}); // FIXME: location?
                else
                    node.outputPlugIndex = OutputPlugIndex{index};
            }
        }
    }

    void GraphCompiler::processWires()
    {
        for (Wire& wire : wires_)
        {
        }
    }

    class GraphCompiler::ExpressionCompilerHost final : public dsExpressionCompilerHost
    {
    public:
        explicit ExpressionCompilerHost(GraphCompiler& compiler) noexcept : compiler_(compiler) {}

        bool lookupVariable(dsName name, dsVariableCompileMeta& out_meta) const noexcept override;

        bool lookupFunction(dsName name, dsFunctionCompileMeta& out_meta) const noexcept override
        {
            return compiler_.host_.lookupFunction(name, out_meta);
        }

    private:
        GraphCompiler& compiler_;
    };

    class GraphCompiler::ExpressionBuilder final : public dsExpressionBuilder
    {
    public:
        explicit ExpressionBuilder(dsAllocator& alloc, GraphCompiler& compiler) noexcept : usedVariables_(alloc), compiler_(compiler) {}

        void bindSlot(InputSlotIndex slotIndex)
        {
            usedVariables_.clear();
            slotIndex_ = slotIndex;
        }

        void pushOp(uint8_t byte) override { compiler_.byteCode_.pushBack(byte); }
        uint32_t pushConstant(dsValueRef const& value) override;
        uint32_t pushFunction(dsFunctionId functionId) override;
        uint32_t pushVariable(uint64_t nameHash) override;

    private:
        dsArray<uint64_t> usedVariables_;
        GraphCompiler& compiler_;
        InputSlotIndex slotIndex_ = dsInvalidIndex;
    };

    void GraphCompiler::compileBindings()
    {
        ExpressionCompilerHost host(*this);
        ExpressionBuilder builder(allocator_, *this);

        // create expression compiler on demand; we'll cache and reuse across graph compiles
        if (exprCompiler_ == nullptr)
            exprCompiler_ = dsCreateExpressionCompiler(allocator_, host);

        for (InputBinding const& binding : inputBindings_)
        {
            if (!binding.live)
                continue;

            InputSlot& slot = inputSlots_[binding.slotIndex];
            slot.live = true;

            if (binding.variableIndex != dsInvalidIndex)
            {
                DS_ASSERT(binding.expressionIndex == dsInvalidIndex);
                DS_ASSERT(binding.constantIndex == dsInvalidIndex);

                Variable& variable = variables_[binding.variableIndex];

                if (variable.type != slot.type)
                {
                    error({.code = dsCompileErrorCode::IncompatibleType});
                    continue;
                }

                variable.live = true;

                DependencyIndex const depIndex{dependencies_.size()};
                dependencies_.pushBack(Dependency{.slotIndex = binding.slotIndex, .nextDependency = variable.firstDependency});
                variable.firstDependency = depIndex;

                ++variable.dependencyCount;
            }
            else if (binding.expressionIndex != dsInvalidIndex)
            {
                DS_ASSERT(binding.variableIndex == dsInvalidIndex);
                DS_ASSERT(binding.constantIndex == dsInvalidIndex);

                Expression& expression = expressions_[binding.expressionIndex];

                builder.bindSlot(binding.slotIndex);
                if (!exprCompiler_->compile(expression.expression.cStr()))
                {
                    error({.code = dsCompileErrorCode::ExpressionCompileError}); // FIXME: location
                    continue;
                }

                if (exprCompiler_->isEmpty())
                    continue;

                if (exprCompiler_->resultType() != slot.type)
                {
                    error({.code = dsCompileErrorCode::IncompatibleType});
                    continue;
                }

                if (!exprCompiler_->optimize())
                {
                    error({.code = dsCompileErrorCode::ExpressionCompileError}); // FIXME: location
                    continue;
                }

                expression.byteCodeStart = dsAssemblyByteCodeIndex{byteCode_.size()};

                if (!exprCompiler_->build(builder))
                {
                    error({.code = dsCompileErrorCode::ExpressionCompileError}); // FIXME: location
                    continue;
                }

                expression.live = true;
                expression.byteCodeCount = byteCode_.size() - expression.byteCodeStart.value();
            }
            else if (binding.constantIndex != dsInvalidIndex)
            {
                DS_ASSERT(binding.variableIndex == dsInvalidIndex);
                DS_ASSERT(binding.expressionIndex == dsInvalidIndex);

                if (constants_[binding.constantIndex].type() != slot.type)
                {
                    error({.code = dsCompileErrorCode::IncompatibleType});
                    continue;
                }
            }
        }

        for (OutputBinding const& binding : outputBindings_)
        {
            if (!binding.live)
                continue;

            OutputSlot& slot = outputSlots_[binding.slotIndex];
            slot.live = true;

            if (binding.variableIndex != dsInvalidIndex)
            {
                Variable& variable = variables_[binding.variableIndex];
                variable.live = true;
            }
        }
    }

    void GraphCompiler::allocateIndices()
    {
        compiledNodeCount_ = 0;
        compiledInputPlugCount_ = 0;
        compiledOutputPlugCount_ = 0;
        compiledWireCount_ = 0;
        compiledInputSlotCount_ = 0;
        compiledOutputSlotCount_ = 0;
        compiledVariableCount_ = 0;
        compiledDependencyCount_ = 0;
        compiledExpressionCount_ = 0;

        // allocate indices for all live variables
        for (Variable& var : variables_)
        {
            if (!var.live)
                continue;

            var.index = dsAssemblyVariableIndex{compiledVariableCount_++};

            // assign indices to dependencies; we already have the count
            var.dependencyStart = dsAssemblyDependencyIndex{compiledDependencyCount_};
            for (DependencyIndex depIndex = var.firstDependency; dependencies_.contains(depIndex);
                 depIndex = dependencies_[depIndex].nextDependency)
            {
                Dependency& dep = dependencies_[depIndex];
                dep.index = dsAssemblyDependencyIndex{compiledDependencyCount_++};
            }
            DS_ASSERT(var.dependencyCount == compiledDependencyCount_ - var.dependencyStart.value());
        }

        // allocate indices for all lives nodes and live output plugs
        for (auto&& [index, node] : dsEnumerate(nodes_))
        {
            if (!node.live)
                continue;

            DS_ASSERT(node.index == dsInvalidIndex);
            node.index = dsAssemblyNodeIndex{compiledNodeCount_++};

            // handle default output plug, which must always be in index 0
            if (node.outputPlugIndex != dsInvalidIndex)
            {
                OutputPlug& plug = outputPlugs_[node.outputPlugIndex];
                if (plug.live)
                    plug.index = dsAssemblyOutputPlugIndex{compiledOutputPlugCount_++};
            }

            // count live input plugs
            for (InputPlugIndex plugKey = node.firstInputPlug; inputPlugs_.contains(plugKey); plugKey = inputPlugs_[plugKey].nextPlug)
            {
                InputPlug& plug = inputPlugs_[plugKey];
                if (!plug.live)
                    continue;

                if (plug.inputPlugIndex != dsBeginPlugIndex && plug.inputPlugIndex.value() >= node.inputPlugCount)
                    node.inputPlugCount = plug.inputPlugIndex.value() + 1;
            }

            compiledInputPlugCount_ += node.inputPlugCount;

            // allocate custom output plugs
            node.outputPlugStart = dsAssemblyOutputPlugIndex{compiledOutputPlugCount_};
            for (OutputPlugIndex plugKey = node.firstOutputPlug; outputPlugs_.contains(plugKey); plugKey = outputPlugs_[plugKey].nextPlug)
            {
                OutputPlug& plug = outputPlugs_[plugKey];
                if (!plug.live)
                    continue;

                if (plug.outputPlugIndex != dsDefaultOutputPlugIndex)
                {
                    plug.index = node.outputPlugStart + plug.outputPlugIndex.value();
                    if (plug.outputPlugIndex.value() >= node.outputPlugCount)
                        node.outputPlugCount = plug.outputPlugIndex.value() + 1;
                }
            }

            compiledOutputPlugCount_ += node.outputPlugCount;

            // allocate input slots
            node.inputSlotStart = dsAssemblyInputSlotIndex{compiledInputSlotCount_};
            for (InputSlotIndex slotIndex = node.firstInputSlot; inputSlots_.contains(slotIndex);
                 slotIndex = inputSlots_[slotIndex].nextSlot)
            {
                InputSlot& slot = inputSlots_[slotIndex];
                if (slot.live)
                {
                    uint8_t const index = slot.inputSlot.value();
                    slot.index = node.inputSlotStart + index;
                    if (index >= node.inputSlotCount)
                        node.inputSlotCount = index + 1;
                }
            }

            compiledInputSlotCount_ += node.inputSlotCount;

            // allocate output slots
            node.outputSlotStart = dsAssemblyOutputSlotIndex{compiledOutputSlotCount_};
            for (OutputSlotIndex slotIndex = node.firstOutputSlot; outputSlots_.contains(slotIndex);
                 slotIndex = outputSlots_[slotIndex].nextSlot)
            {
                OutputSlot& slot = outputSlots_[slotIndex];
                if (slot.live)
                {
                    uint8_t const index = slot.outputSlot.value();
                    slot.index = node.outputSlotStart + index;
                    if (index >= node.outputSlotCount)
                        node.outputSlotCount = index + 1;
                }
            }

            compiledOutputSlotCount_ += node.outputSlotCount;
        }

        // allocate indices for all wires and assign target information
        for (auto&& [index, plug] : dsEnumerate(outputPlugs_))
        {
            if (!plug.live)
                continue;

            plug.wireStart = dsAssemblyWireIndex{compiledWireCount_};

            for (PlugWireLinkIndex linkIndex = plug.firstLink; plugWireLinks_.contains(linkIndex);
                 linkIndex = plugWireLinks_[linkIndex].nextLink)
            {
                WireIndex const wireIndex = plugWireLinks_[linkIndex].wireIndex;
                Wire& wire = wires_[wireIndex];

                DS_ASSERT(wire.index == dsInvalidIndex);
                wire.index = dsAssemblyWireIndex{compiledWireCount_++};
            }

            plug.wireCount = compiledWireCount_ - plug.wireStart.value();
        }

        // allocate indices for all expressions
        for (auto&& [index, expression] : dsEnumerate(expressions_))
        {
            if (!expression.live)
                continue;

            expression.index = dsAssemblyExpressionIndex{compiledExpressionCount_++};
        }
    }

    bool GraphCompiler::error(dsCompileError const& error)
    {
        errors_.pushBack(error);
        return false;
    }

    GraphCompiler::NodeIndex GraphCompiler::findNode(dsNodeId nodeId) const noexcept
    {
        for (auto&& [index, node] : dsEnumerate(nodes_))
            if (node.nodeId == nodeId)
                return NodeIndex{index};
        return dsInvalidIndex;
    }

    GraphCompiler::InputPlugIndex GraphCompiler::findPlug(dsNodeId nodeId, dsInputPlugIndex plugIndex) const noexcept
    {
        for (auto&& [index, plug] : dsEnumerate(inputPlugs_))
            if (plug.nodeId == nodeId && plug.inputPlugIndex == plugIndex)
                return InputPlugIndex{index};
        return dsInvalidIndex;
    }

    GraphCompiler::OutputPlugIndex GraphCompiler::findPlug(dsNodeId nodeId, dsOutputPlugIndex plugIndex) const noexcept
    {
        for (auto&& [index, plug] : dsEnumerate(outputPlugs_))
            if (plug.nodeId == nodeId && plug.outputPlugIndex == plugIndex)
                return OutputPlugIndex{index};
        return dsInvalidIndex;
    }

    uint32_t GraphCompiler::ExpressionBuilder::pushVariable(uint64_t nameHash)
    {
        for (auto&& [index, variable] : dsEnumerate(compiler_.variables_))
        {
            if (variable.nameHash != nameHash)
                continue;

            variable.live = true;

            if (slotIndex_ != dsInvalidIndex)
            {
                bool seen = false;
                for (uint64_t const seenHash : usedVariables_)
                {
                    if (seenHash == nameHash)
                    {
                        seen = true;
                        break;
                    }
                }
                if (!seen)
                {
                    DependencyIndex const depIndex{compiler_.dependencies_.size()};
                    compiler_.dependencies_.pushBack(Dependency{.slotIndex = slotIndex_, .nextDependency = variable.firstDependency});
                    variable.firstDependency = depIndex;

                    ++variable.dependencyCount;
                }
            }

            return index;
        }

        DS_ASSERT(false, "Resolved unknown variable id");
        return 0;
    }

    uint32_t GraphCompiler::ExpressionBuilder::pushConstant(dsValueRef const& value)
    {
        for (auto&& [index, constant] : dsEnumerate(compiler_.constants_))
            if (value == constant.ref())
                return index;

        uint32_t const index{compiler_.constants_.size()};
        compiler_.constants_.emplaceBack(value);
        return index;
    }

    uint32_t GraphCompiler::ExpressionBuilder::pushFunction(dsFunctionId functionId)
    {
        for (auto&& [index, function] : dsEnumerate(compiler_.functions_))
            if (function == functionId)
                return index;

        uint32_t const index{compiler_.functions_.size()};
        compiler_.functions_.pushBack(functionId);
        return index;
    }

    bool GraphCompiler::ExpressionCompilerHost::lookupVariable(dsName name, dsVariableCompileMeta& out_meta) const noexcept
    {
        uint64_t const nameHash = dsHashFnv1a64(name.name, name.nameEnd);
        for (Variable const& var : compiler_.variables_)
        {
            if (var.nameHash == nameHash)
            {
                out_meta.type = var.type;
                return true;
            }
        }

        return false;
    }

} // namespace descript
