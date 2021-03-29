// descript

#include "graph.hh"

#include <imnodes.h>

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace descript::sample {

    GraphNode* Graph::createNode(dsUuid const& uuid, std::string_view title, ImVec2 pos)
    {
        auto node = std::make_unique<GraphNode>();
        node->uuid = uuid;
        node->title = title;
        node->pos = pos;
        node->id = nextId_++;
        nodes_.push_back(std::move(node));
        return nodes_.back().get();
    }

    GraphPlug* Graph::createPlug(dsUuid const& uuid, dsUuid const& node, dsPlugKind kind, std::string_view title)
    {
        auto const nodeIt = std::find_if(nodes_.begin(), nodes_.end(), [&node](auto& item) { return item->uuid == node; });
        assert(nodeIt != nodes_.end());

        auto plug = std::make_unique<GraphPlug>();
        plug->uuid = uuid;
        plug->node = nodeIt->get();
        plug->plugKind = kind;
        plug->title = title;
        plug->id = nextId_++;
        (*nodeIt)->plugs.push_back(plug.get());
        plugs_.push_back(std::move(plug));
        return plugs_.back().get();
    }

    GraphWire* Graph::createWire(dsUuid const& uuid, dsUuid const& from, dsUuid const& to)
    {
        auto const fromIt = std::find_if(plugs_.begin(), plugs_.end(), [&from](auto& item) { return item->uuid == from; });
        assert(fromIt != plugs_.end());
        auto const toIt = std::find_if(plugs_.begin(), plugs_.end(), [&to](auto& item) { return item->uuid == to; });
        assert(toIt != plugs_.end());

        auto wire = std::make_unique<GraphWire>();
        wire->uuid = uuid;
        wire->id = nextId_++;
        wire->fromPlug = fromIt->get();
        wire->toPlug = toIt->get();
        wire->fromPlug->wires.push_back(wire.get());
        wire->toPlug->wires.push_back(wire.get());
        wires_.push_back(std::move(wire));
        return wires_.back().get();
    }

    void Graph::deleteNode(int nodeId)
    {
        auto const it = std::find_if(nodes_.begin(), nodes_.end(), [nodeId](auto& item) { return item->id == nodeId; });
        if (it != nodes_.end())
        {
            for (auto* plug : (*it)->plugs)
            {
                deletePlug(plug->id);
            }

            nodes_.erase(it);
        }
    }

    void Graph::deletePlug(int plugId)
    {
        auto const it = std::find_if(plugs_.begin(), plugs_.end(), [plugId](auto& item) { return item->id == plugId; });
        if (it != plugs_.end())
        {
            auto const ownerIt = std::find((*it)->node->plugs.begin(), (*it)->node->plugs.end(), it->get());
            if (ownerIt != (*it)->node->plugs.end())
            {
                (*it)->node->plugs.erase(ownerIt);
            }

            for (auto wireIt = wires_.begin(); wireIt != wires_.end();)
            {
                if ((*wireIt)->fromPlug == it->get() || (*wireIt)->toPlug == it->get())
                {
                    wireIt = wires_.erase(wireIt);
                }
                else
                {
                    ++wireIt;
                }
            }

            plugs_.erase(it);
        }
    }

    void Graph::deleteWire(int wireId)
    {
        auto const it = std::find_if(wires_.begin(), wires_.end(), [wireId](auto& item) { return item->id == wireId; });
        if (it != wires_.end())
        {
            auto const fromIt = std::find((*it)->fromPlug->wires.begin(), (*it)->fromPlug->wires.end(), it->get());
            if (fromIt != (*it)->fromPlug->wires.end())
            {
                (*it)->fromPlug->wires.erase(fromIt);
            }

            auto const toIt = std::find((*it)->toPlug->wires.begin(), (*it)->toPlug->wires.end(), it->get());
            if (toIt != (*it)->toPlug->wires.end())
            {
                (*it)->toPlug->wires.erase(toIt);
            }

            wires_.erase(it);
        }
    }

    void Graph::draw()
    {
        for (std::unique_ptr<GraphNode>& node : nodes_)
        {
            ImNodes::SetNodeGridSpacePos(node->id, node->pos);
            ImNodes::SetNodeDraggable(node->id, true);
            ImNodes::BeginNode(node->id);

            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(node->title.c_str());
            ImNodes::EndNodeTitleBar();

            for (GraphPlug* plug : node->plugs)
            {
                const bool isOutput = plug->plugKind == dsPlugKind::Output || plug->plugKind == dsPlugKind::CustomOutput;
                const bool hasWire = !plug->wires.empty();
                const ImNodesPinShape shape = hasWire ? ImNodesPinShape_QuadFilled : ImNodesPinShape_Quad;
                if (isOutput)
                {
                    ImNodes::BeginOutputAttribute(plug->id, shape);
                }
                else
                {
                    ImNodes::BeginInputAttribute(plug->id, shape);
                }

                ImGui::TextUnformatted(plug->title.c_str());

                if (isOutput)
                {
                    ImNodes::EndOutputAttribute();
                }
                else
                {
                    ImNodes::EndInputAttribute();
                }
            }

            ImNodes::EndNode();

            ImVec2 const pos = ImNodes::GetNodeGridSpacePos(node->id);
            node->pos.x = pos.x;
            node->pos.y = pos.y;
        }

        for (std::unique_ptr<GraphWire>& wire : wires_)
        {
            ImNodes::Link(wire->id, wire->fromPlug->id, wire->toPlug->id);
        }
    }

    void Graph::apply()
    {
        for (std::unique_ptr<GraphNode>& node : nodes_)
        {
            node->pos = ImNodes::GetNodeGridSpacePos(node->id);
        }

        {
            int wire = -1;
            if (ImNodes::IsLinkDestroyed(&wire))
            {
                deleteWire(wire);
            }
        }

        {
            int from = -1;
            int to = -1;
            if (ImNodes::IsLinkCreated(&from, &to))
            {
                auto fromIt = std::find_if(plugs_.begin(), plugs_.end(), [from](auto& item) { return item->id == from; });
                auto toIt = std::find_if(plugs_.begin(), plugs_.end(), [to](auto& item) { return item->id == to; });
                createWire(dsCreateUuid(), (*fromIt)->uuid, (*toIt)->uuid);
            }
        }
    }
} // namespace descript::sample
