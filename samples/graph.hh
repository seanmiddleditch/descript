// descript

#pragma once

#include "descript/types.hh"
#include "descript/uuid.hh"

#include <imgui.h>

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace descript::sample {
    struct GraphNode;
    struct GraphPlug;
    struct GraphWire;

    struct GraphNode
    {
        dsUuid uuid;
        int id = -1;
        std::string title;
        ImVec2 pos;
        std::vector<GraphPlug*> plugs;
    };

    struct GraphPlug
    {
        dsUuid uuid;
        int id = -1;
        std::string title;
        GraphNode* node = nullptr;
        dsPlugKind plugKind = dsPlugKind::Begin;
        std::vector<GraphWire*> wires;
    };

    struct GraphWire
    {
        dsUuid uuid;
        int id = -1;
        GraphPlug* fromPlug = nullptr;
        GraphPlug* toPlug = nullptr;
    };

    class Graph
    {
    public:
        Graph(dsUuid const& uuid) : uuid_(uuid) {}

        GraphNode* createNode(dsUuid const& uuid, std::string_view title, ImVec2 pos);
        GraphPlug* createPlug(dsUuid const& uuid, dsUuid const& node, dsPlugKind kind, std::string_view title);
        GraphWire* createWire(dsUuid const& uuid, dsUuid const& from, dsUuid const& to);

        void deleteNode(int nodeId);
        void deletePlug(int nodeId);
        void deleteWire(int wireId);

        void draw();
        void apply();

    private:
        dsUuid uuid_;
        std::vector<std::unique_ptr<GraphNode>> nodes_;
        std::vector<std::unique_ptr<GraphPlug>> plugs_;
        std::vector<std::unique_ptr<GraphWire>> wires_;
        int nextId_ = 0;
    };
} // namespace descript::sample
