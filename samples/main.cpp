// descript

#include "graph.hh"
#include "schema.hh"

#include <descript/alloc.hh>
#include <descript/compiler.hh>
#include <descript/runtime.hh>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>

#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

using namespace descript;

namespace ned = ax::NodeEditor;

namespace std {
    template <>
    struct formatter<sample::graph::Node> : std::formatter<std::string_view>
    {
        inline auto format(sample::graph::Node const& node, format_context& ctx);
    };

    template <>
    struct formatter<sample::graph::Plug> : std::formatter<std::string_view>
    {
        inline auto format(sample::graph::Plug const& plug, format_context& ctx);
    };

        template <>
    struct formatter<sample::graph::Link> : std::formatter<std::string_view>
    {
            inline auto format(sample::graph::Link const& link, format_context& ctx);
    };
} // namespace std

namespace sample::graph {
    std::ostream& operator<<(std::ostream& os, graph::Node const& node);
    std::ostream& operator<<(std::ostream& os, graph::Plug const& plug);
} // namespace sample::graph

namespace sample {
    class App
    {
    public:
        int run(int argc, char** argv);

    private:
        class CompileHost;
        class RuntimeHost;

        void createSchema();
        void createGraph();

        void compileGraph();

        void drawMenu();
        bool drawWindow();
        void drawLog();
        bool drawVariables();
        bool drawGraph();

        template <typename... Args>
        void log(std::string_view format, Args&&... args)
        {
            logs_.push_back(std::vformat(format, std::make_format_args(std::forward<Args>(args)...)));
        }

        GLFWwindow* window_ = nullptr;
        ned::EditorContext* nodeEditorCtx_ = nullptr;
        std::unique_ptr<graph::Graph> graph_;
        std::unique_ptr<schema::Schema> schema_;
        ned::PinId newNodeFromPin_;
        std::vector<std::string> logs_;
        std::unique_ptr<RuntimeHost> host_;
        std::unique_ptr<dsRuntime, decltype(&dsDestroyRuntime)> runtime_ = {nullptr, &dsDestroyRuntime};
    };

    class App::CompileHost final : public descript::dsCompilerHost
    {
    public:
        explicit CompileHost(App& app) noexcept : app_(app) {}

        bool lookupNodeType(dsNodeTypeId typeId, dsNodeCompileMeta& out_nodeMeta) const noexcept override;
        bool lookupFunction(dsName name, dsFunctionCompileMeta& out_functionMeta) const noexcept override;
        void onError(dsCompileError const& error) override;

    private:
        App& app_;
    };

    class App::RuntimeHost final : public descript::dsRuntimeHost
    {
    public:
        explicit RuntimeHost(App& app) noexcept : app_(app) {}

        bool lookupNode(dsNodeTypeId, dsNodeRuntimeMeta& out_meta) const noexcept override;
        bool lookupFunction(dsFunctionId, dsFunctionRuntimeMeta& out_meta) const noexcept override;

    private:
        App& app_;
    };

    bool App::CompileHost::lookupNodeType(dsNodeTypeId typeId, dsNodeCompileMeta& out_nodeMeta) const noexcept
    {
        for (auto const& nodeSchemaPtr : app_.schema_->nodes)
        {
            if (nodeSchemaPtr->typeId == typeId.value())
            {
                out_nodeMeta.typeId = typeId;
                out_nodeMeta.kind = nodeSchemaPtr->kind == schema::NodeKind::Entry ? dsNodeKind::Entry : dsNodeKind::State;
                return true;
            }
        }
        return false;
    }

    bool App::CompileHost::lookupFunction(dsName name, dsFunctionCompileMeta& out_functionMeta) const noexcept { return false; }

    void App::CompileHost::onError(dsCompileError const& error) { app_.log("Error {}", (int)error.code); }

    bool App::RuntimeHost::lookupNode(dsNodeTypeId, dsNodeRuntimeMeta& out_meta) const noexcept { return false; }

    bool App::RuntimeHost::lookupFunction(dsFunctionId, dsFunctionRuntimeMeta& out_meta) const noexcept { return false; }

    int App::run(int argc, char** argv)
    {
        if (!glfwInit())
            return 1;

        window_ = glfwCreateWindow(1024, 768, "Descript Sample", nullptr, nullptr);
        if (window_ == nullptr)
            return 2;

        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init();

        ned::Config config;
        nodeEditorCtx_ = ned::CreateEditor(&config);

        createSchema();
        createGraph();

        bool dirty = true;

        while (!glfwWindowShouldClose(window_))
        {
            if (dirty)
            {
                dirty = false;
                compileGraph();
            }

            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            dirty |= drawWindow();

            ImGui::EndFrame();
            ImGui::Render();

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window_);
        }

        ned::DestroyEditor(nodeEditorCtx_);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window_);
        glfwTerminate();

        return 0;
    }

    void App::createSchema()
    {
        using namespace schema;

        schema_ = std::make_unique<Schema>();

        log("Creating schema");

        {
            Node& entry = schema_->createNode("entry", NodeKind::Entry);
            entry.title = "Entry";
            schema_->createPlug(entry, "out", PlugKind::Default, 0);
        }

        {
            Node& condition = schema_->createNode("condition", NodeKind::State);
            condition.title = "Condition";
            schema_->createPlug(condition, "begin", PlugKind::Begin, 0);
            schema_->createPlug(condition, "out", PlugKind::Default, 0);
            schema_->createPlug(condition, "in0", PlugKind::Input, 0);
            schema_->createPlug(condition, "out0", PlugKind::Output, 0);
            schema_->createSlot(condition, "keyval", SlotKind::Pair, 0);
        }

        {
            Node& print = schema_->createNode("print", NodeKind::State);
            print.title = "Print";
            schema_->createPlug(print, "begin", PlugKind::Begin, 0);
            schema_->createPlug(print, "out", PlugKind::Default, 0);
            schema_->createPlug(print, "in0", PlugKind::Input, 0);
        }
    }

    void App::createGraph()
    {
        using namespace graph;

        log("Creating graph");

        graph_ = std::make_unique<graph::Graph>(schema_.get());

        Node* entry = graph_->createNode("entry");
        Node* when = graph_->createNode("condition");
        Node* print = graph_->createNode("print");

        graph_->createLink(entry->plugs.front().get(), when->plugs.front().get());
    }

    void App::compileGraph()
    {
        CompileHost host(*this);
        dsDefaultAllocator alloc;
        dsGraphCompiler* compiler = dsCreateGraphCompiler(alloc, host);

        log("Compiling...");

        for (auto const& varPtr : graph_->variables)
        {
            if (!varPtr->name.empty())
                compiler->addVariable(dsName(varPtr->name.c_str()), dsValueType::Int32);
        }

        for (auto const& nodePtr : graph_->nodes)
        {
            compiler->beginNode(dsNodeId{(uintptr_t)nodePtr.get()}, dsNodeTypeId{(unsigned)nodePtr->schema->typeId});

            for (auto const& plugPtr : nodePtr->plugs)
            {
                switch (plugPtr->schema->kind)
                {
                case schema::PlugKind::Begin: compiler->addInputPlug(dsBeginPlugIndex); break;
                case schema::PlugKind::Default: compiler->addOutputPlug(dsDefaultOutputPlugIndex); break;
                case schema::PlugKind::Input: compiler->addInputPlug(dsInputPlugIndex{plugPtr->schema->index}); break;
                case schema::PlugKind::Output: compiler->addOutputPlug(dsOutputPlugIndex{plugPtr->schema->index}); break;
                }
            }

            for (auto const& slotPtr : nodePtr->slots)
            {
                compiler->addInputSlot(dsInputSlotIndex{slotPtr->schema->index});
                compiler->bindSlotExpression(dsNodeId{(uintptr_t)nodePtr.get()}, dsInputSlotIndex{slotPtr->schema->index},
                    slotPtr->expression.c_str());
            }
        }

        for (auto const& linkPtr : graph_->links)
        {
            compiler->addWire(dsNodeId((uintptr_t)linkPtr->fromPlug->node),
                linkPtr->fromPlug->schema->kind == schema::PlugKind::Default ? dsDefaultOutputPlugIndex
                                                                             : dsOutputPlugIndex(linkPtr->fromPlug->schema->index),
                dsNodeId((uintptr_t)linkPtr->toPlug->node),
                linkPtr->toPlug->schema->kind == schema::PlugKind::Begin ? dsBeginPlugIndex
                                                                         : dsInputPlugIndex{linkPtr->toPlug->schema->index});
        }

        bool const result = compiler->compile();
        log("Compile {}", result ? "succeeded" : "failed");

        dsDestroyGraphCompiler(compiler);
    }

    void App::drawMenu()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Descript"))
            {
                if (ImGui::MenuItem("Quit", "Alt+F4"))
                {
                    glfwSetWindowShouldClose(window_, 1);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }

    bool App::drawWindow()
    {
        int width = 0, height = 0;
        glfwGetWindowSize(window_, &width, &height);

        ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({(float)width, (float)height}, ImGuiCond_Always);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar);

        drawMenu();

        bool edits = false;
        if (ImGui::BeginTable("##main_split", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("##left_column", ImGuiTableColumnFlags_None, 0.3f);
            ImGui::TableSetupColumn("##right_column", ImGuiTableColumnFlags_None, 0.7f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            edits |= drawVariables();
            drawLog();
            ImGui::TableNextColumn();
            edits |= drawGraph();
            ImGui::EndTable();
        }

        ImGui::End();

        return edits;
    }

    void App::drawLog()
    {
        if (ImGui::BeginTable("##log", 1))
        {
            for (std::string const& log : logs_)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(log.c_str());
            }
            ImGui::EndTable();
        }
    }

    bool App::drawVariables()
    {
        ImVec2 const avail = ImGui::GetContentRegionAvail();

        bool edits = false;

        if (ImGui::BeginChildFrame(ImGui::GetID("##variables"), ImVec2(avail.x, avail.y / 2)))
        {
            char label[32];
            char buffer[128];
            int index = 0;

            for (auto& varPtr : graph_->variables)
            {
                *std::format_to_n(label, sizeof(label) - 1, "Var {}", index++).out = '\0';

                *std::format_to_n(buffer, sizeof(buffer) - 1, "{}", varPtr->name).out = '\0';
                ImGui::InputText(label, buffer, sizeof(buffer));
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    varPtr->name = buffer;
                    edits = true;
                }
            }

            if (ImGui::Button("+ Add"))
            {
                graph_->variables.push_back(std::make_unique<graph::Variable>());
            }
            ImGui::EndChildFrame();
        }

        return edits;
    }

    bool App::drawGraph()
    {
        ned::SetCurrentEditor(nodeEditorCtx_);
        ned::Begin("Descript Sample", ImVec2(0.0, 0.0f));

        bool edits = false;

        ImGuiID menuId = ImGui::GetID("##graph_menu");

        for (auto& nodePtr : graph_->nodes)
        {
            ned::BeginNode(ned::NodeId(nodePtr.get()));
            ImGui::Text("%s#%llx", nodePtr->schema->title.c_str(), (uintptr_t)nodePtr.get());

            for (auto& plugPtr : nodePtr->plugs)
            {
                if (plugPtr->schema->kind == schema::PlugKind::Begin || plugPtr->schema->kind == schema::PlugKind::Input)
                {
                    ned::BeginPin(ned::PinId(plugPtr.get()), ned::PinKind::Input);
                    ned::PinPivotAlignment({0, 0.5});
                }
                else
                {
                    ned::BeginPin(ned::PinId(plugPtr.get()), ned::PinKind::Output);
                    ned::PinPivotAlignment({1, 0.5});
                }

                ImGui::Text("%s(%d)", plugPtr->schema->name.c_str(), plugPtr->schema->index);
                ned::EndPin();
            }

            for (auto& slotPtr : nodePtr->slots)
            {
                ImGui::PushID(slotPtr->schema->index);
                ImGui::PushID((int)slotPtr->schema->kind);

                char buffer[1024] = {};
                *std::format_to_n(buffer, sizeof(buffer) - 1, "{}", slotPtr->expression).out = '\0';
                ImGui::SetNextItemWidth(100);
                ImGui::InputText("##slot", buffer, sizeof(buffer));
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    slotPtr->expression = buffer;
                    edits = true;
                }
                ImGui::PopID();
                ImGui::PopID();
            }

            ned::EndNode();
        }

        for (auto& linkPtr : graph_->links)
        {
            ned::Link(ned::LinkId(linkPtr.get()), ned::PinId(linkPtr->fromPlug), ned::PinId(linkPtr->toPlug));
        }

        if (ned::BeginCreate())
        {
            ned::PinId fromPinId = -1;
            ned::PinId toPinId = -1;
            if (ned::QueryNewLink(&fromPinId, &toPinId))
            {
                graph::Plug* fromPlug = fromPinId.AsPointer<graph::Plug>();
                graph::Plug* toPlug = toPinId.AsPointer<graph::Plug>();

                if (fromPlug == nullptr || toPlug == nullptr)
                {
                    ned::RejectNewItem();
                }
                else if (fromPlug->node == toPlug->node)
                {
                    ned::RejectNewItem();
                }
                else if (schema::IsInputPlug(fromPlug->schema->kind) == schema::IsInputPlug(toPlug->schema->kind))
                {
                    ned::RejectNewItem({1, 0, 0, 1});
                }
                else if (!toPlug->links.empty())
                {
                    ned::RejectNewItem();
                }
                else if (ned::AcceptNewItem())
                {

                    graph::Link* link = graph_->createLink(fromPlug, toPlug);
                    log("Created link {}", *link);
                    edits = true;
                }
            }

            if (ned::QueryNewNode(&fromPinId))
            {
                if (ned::AcceptNewItem())
                {
                    newNodeFromPin_ = fromPinId;
                    ImGui::OpenPopup(menuId);
                }
            }
        }
        ned::EndCreate();

        if (ned::BeginDelete())
        {
            ned::LinkId linkId;
            ned::NodeId nodeId;

            if (ned::QueryDeletedLink(&linkId))
            {
                if (ned::AcceptDeletedItem())
                {
                    graph::Link* link = graph_->findLink(linkId.Get());
                    log("Deleting link {}", *link);
                    graph_->destroyLink(link);
                    edits = true;
                }
            }

            if (ned::QueryDeletedNode(&nodeId))
            {
                if (ned::AcceptDeletedItem())
                {
                    graph::Node* node = graph_->findNode(nodeId.Get());
                    log("Deleting node {}", *node);
                    graph_->destroyNode(node);
                    edits = true;
                }
            }
        }
        ned::EndDelete();

        ned::End();

        if (ned::IsBackgroundClicked())
            ImGui::OpenPopup(menuId);

        if (ImGui::BeginPopupEx(menuId, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImVec2 const mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
            for (auto& nodeSchemaPtr : schema_->nodes)
            {
                if (ImGui::MenuItem(nodeSchemaPtr->title.c_str()))
                {
                    graph::Node* node = graph_->createNode(nodeSchemaPtr->name);
                    log("Created node {}", *node);
                    ned::SetNodePosition(ned::NodeId(node), ned::ScreenToCanvas(mousePos));
                    if (newNodeFromPin_)
                    {
                        graph::Plug* fromPlug = newNodeFromPin_.AsPointer<graph::Plug>();
                        graph::Link* link = graph_->createLink(fromPlug, node->plugs.front().get());
                        log("Created link {}", *link);
                    }
                    newNodeFromPin_ = {};
                    edits = true;
                }
            }
            ImGui::EndPopup();
        }

        ned::SetCurrentEditor(nullptr);

        return edits;
    }
} // namespace sample

int main(int argc, char** argv)
{
    sample::App app;
    return app.run(argc, argv);
}

auto std::formatter<sample::graph::Node>::format(sample::graph::Node const& node, format_context& ctx)
{
    return std::format_to(ctx.out(), "{}#{:x}", node.schema->title, (uintptr_t)&node);
}

auto std::formatter<sample::graph::Plug>::format(sample::graph::Plug const& plug, format_context& ctx)
{
    switch (plug.schema->kind)
    {
    case sample::schema::PlugKind::Begin: return format_to(ctx.out(), "Begin");
    case sample::schema::PlugKind::Input: return format_to(ctx.out(), "Input({})", plug.schema->index);
    case sample::schema::PlugKind::Default: return format_to(ctx.out(), "Default");
    case sample::schema::PlugKind::Output: return format_to(ctx.out(), "Output({})", plug.schema->index);
    default: return ctx.out();
    }
}

auto std::formatter<sample::graph::Link>::format(sample::graph::Link const& link, format_context& ctx)
{
    return std::format_to(ctx.out(), "{}:{} to {}:{}", *link.fromPlug->node, *link.fromPlug, *link.toPlug->node, *link.toPlug->node);
}
