// descript

#include "graph.hh"

#include <descript/uuid.hh>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imnodes.h>

#include <memory>

using namespace descript;
using namespace descript::sample;

static GLFWwindow* gWindow = nullptr;
static std::unique_ptr<Graph> gGraph;

static void dsMainMenu();
static void dsDrawWindow();
static void dsDrawGraph();
static void dsCreateGraph();

static void dsMainMenu()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Descript"))
        {
            if (ImGui::MenuItem("Quit", "Alt+F4"))
            {
                glfwSetWindowShouldClose(gWindow, 1);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

static void dsDrawWindow()
{
    int width = 0, height = 0;
    glfwGetWindowSize(gWindow, &width, &height);

    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)width, (float)height}, ImGuiCond_Always);
    ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar);

    dsMainMenu();
    dsDrawGraph();

    ImGui::End();
}

static void dsDrawGraph()
{
    ImNodes::BeginNodeEditor();

    if (gGraph)
    {
        gGraph->draw();
    }

    ImNodes::EndNodeEditor();

    if (gGraph)
    {
        gGraph->apply();
    }
}

static void dsCreateGraph()
{
    const dsUuid graphUuid = dsCreateUuid();

    const dsUuid entryNodeUuid = dsCreateUuid();
    const dsUuid conditionNodeUuid = dsCreateUuid();
    const dsUuid printNodeUuid = dsCreateUuid();

    const dsUuid entryOutputPlugUuid = dsCreateUuid();
    const dsUuid conditionBeginPlugUuid = dsCreateUuid();
    const dsUuid conditionOutputPlugUuid = dsCreateUuid();
    const dsUuid printBeginPlugUuid = dsCreateUuid();

    const dsUuid entryConditionWirePlugUuid = dsCreateUuid();

    gGraph = std::make_unique<Graph>(graphUuid);

    int imNodeId = 0;
    int imPlugId = 0;
    int imWireId = 0;

    gGraph->createNode(entryNodeUuid, "Entry", {50, 50});
    gGraph->createPlug(entryOutputPlugUuid, entryNodeUuid, dsPlugKind::Output, "Output");

    gGraph->createNode(conditionNodeUuid, "Condition", {250, 50});
    gGraph->createPlug(conditionBeginPlugUuid, conditionNodeUuid, dsPlugKind::Begin, "Begin");
    gGraph->createPlug(conditionOutputPlugUuid, conditionNodeUuid, dsPlugKind::Output, "Output");

    gGraph->createNode(printNodeUuid, "Print", {250, 150});
    gGraph->createPlug(printBeginPlugUuid, printNodeUuid, dsPlugKind::Begin, "Begin");

    gGraph->createWire(entryConditionWirePlugUuid, entryOutputPlugUuid, conditionBeginPlugUuid);
}

int main(int argc, char** argv)
{
    if (!glfwInit())
    {
        return 1;
    }

    gWindow = glfwCreateWindow(1024, 768, "Descript Sample", nullptr, nullptr);
    if (!gWindow)
    {
        return 2;
    }

    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(gWindow, true);
    ImGui_ImplOpenGL3_Init();
    ImNodes::CreateContext();

    dsCreateGraph();

    while (!glfwWindowShouldClose(gWindow))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        dsDrawWindow();

        ImGui::EndFrame();
        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(gWindow);
    }

    ImNodes::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(gWindow);
    glfwTerminate();

    return 0;
}
