// descript

#include "graph.hh"
#include "schema.hh"

#include <descript/alloc.hh>
#include <descript/graph_compiler.hh>
#include <descript/runtime.hh>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <imgui_stdlib.h>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

using namespace descript;

namespace ned = ax::NodeEditor;

namespace sample::graph {
    std::ostream& operator<<(std::ostream& os, Node const& node);
    std::ostream& operator<<(std::ostream& os, Plug const& plug);
    std::ostream& operator<<(std::ostream& os, Link const& link);
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
        void log(Args const&... args)
        {
            std::ostringstream os;
            (os << ... << args);
            logs_.push_back(os.str());
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

    class App::CompileHost final : public descript::dsGraphCompilerHost
    {
    public:
        explicit CompileHost(App& app) noexcept : app_(app) {}

        bool lookupNodeType(dsNodeTypeId typeId, dsNodeCompileMeta& out_nodeMeta) const noexcept override;
        bool lookupFunction(dsName name, dsFunctionCompileMeta& out_functionMeta) const noexcept override;

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
            schema_->createSlot(condition, "test", SlotKind::Input, 0);
        }

        {
            Node& condition = schema_->createNode("set", NodeKind::State);
            condition.title = "Set";
            schema_->createPlug(condition, "begin", PlugKind::Begin, 0);
            schema_->createPlug(condition, "out", PlugKind::Default, 0);
            schema_->createSlot(condition, "dest", SlotKind::Output, 0);
            schema_->createSlot(condition, "source", SlotKind::Input, 0);
        }

        {
            Node& print = schema_->createNode("print", NodeKind::State);
            print.title = "Print";
            schema_->createPlug(print, "begin", PlugKind::Begin, 0);
            schema_->createPlug(print, "out", PlugKind::Default, 0);
            schema_->createPlug(print, "in0", PlugKind::Input, 0);
            schema_->createSlot(print, "value", SlotKind::Input, 0);
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
                compiler->addVariable(dsType<int32_t>.typeId, varPtr->name.c_str());
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
                compiler->beginInputSlot(dsInputSlot(slotPtr->schema->index), dsType<int32_t>.typeId);
                {
                    compiler->bindExpression(slotPtr->expression.c_str());
                }
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
        log("Compile ", result ? "succeeded" : "failed");

        for (uint32_t index = 0, count = compiler->getErrorCount(); index != count; ++index)
        {
            dsCompileError const error = compiler->getError(index);
            log("Error ", (int)error.code);
        }

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
            std::ostringstream label;
            std::ostringstream buffer;
            int index = 0;

            for (auto& varPtr : graph_->variables)
            {
                label.clear();
                label << "Var " << index++;

                ImGui::InputText(label.str().c_str(), &varPtr->name, sizeof(buffer));
                if (ImGui::IsItemDeactivatedAfterEdit())
                    edits = true;
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
            ImGui::PushID(nodePtr.get());
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

                ImGui::SetNextItemWidth(100);
                ImGui::InputText("##slot", &slotPtr->expression);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    edits = true;
                ImGui::PopID();
                ImGui::PopID();
            }

            ImGui::PopID();
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
                    if (link != nullptr)
                    {
                        log("Created link ", *link);
                        edits = true;
                    }
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
                    graph::Link* link = linkId.AsPointer<graph::Link>();
                    log("Deleting link ", *link);
                    graph_->destroyLink(link);
                    edits = true;
                }
            }

            if (ned::QueryDeletedNode(&nodeId))
            {
                if (ned::AcceptDeletedItem())
                {
                    graph::Node* node = nodeId.AsPointer<graph::Node>();
                    log("Deleting node ", *node);
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
                    log("Created node ", *node);
                    ned::SetNodePosition(ned::NodeId(node), ned::ScreenToCanvas(mousePos));
                    if (newNodeFromPin_)
                    {
                        graph::Plug* fromPlug = newNodeFromPin_.AsPointer<graph::Plug>();
                        graph::Link* link = graph_->createLink(fromPlug, node->plugs.front().get());
                        log("Created link ", *link);
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

std::ostream& sample::graph::operator<<(std::ostream& os, Node const& node)
{
    return os << node.schema->title << "#" << &node;
}

std::ostream& sample::graph::operator<<(std::ostream& os, Plug const& plug)
{
    switch (plug.schema->kind)
    {
    case sample::schema::PlugKind::Begin: return os << "Begin";
    case sample::schema::PlugKind::Input: return os << "Input(" << plug.schema->index << ')';
    case sample::schema::PlugKind::Default: return os << "Default";
    case sample::schema::PlugKind::Output: return os << "Output(" << plug.schema->index << ')';
    default: return os;
    }
}

std::ostream& sample::graph::operator<<(std::ostream& os, Link const& link)
{
    return os << *link.fromPlug->node << ':' << *link.fromPlug << " to " << *link.toPlug->node << ':' << *link.toPlug->node;
}
