#pragma once

#include "schema.hh"

#include <string>

namespace sample::graph {
    struct Node;
    struct Plug;
    struct Slot;
    struct Link;

    struct Plug
    {
        Node* node = nullptr;
        schema::Plug const* schema = nullptr;
        std::vector<Link*> links;
        int id = -1;
    };

    struct Slot
    {
        Node* node = nullptr;
        schema::Slot const* schema = nullptr;
        std::string expression;
    };

    struct Node
    {
        schema::Node const* schema = nullptr;
        std::vector<std::unique_ptr<Plug>> plugs;
        std::vector<std::unique_ptr<Slot>> slots;
        int id = -1;
    };

    struct Link
    {
        Plug* fromPlug = nullptr;
        Plug* toPlug = nullptr;
        int id = -1;
    };

    struct Variable
    {
        std::string name;
    };

    struct Graph
    {
        explicit Graph(schema::Schema const* schema) noexcept : schema(schema) {}

        inline Node* findNode(int nodeId);
        inline Plug* findPlug(int plugId);
        inline Link* findLink(int linkId);

        inline void destroyNode(Node* node);
        inline void destroyLink(Link* node);

        inline Node* createNode(std::string_view name);
        inline Link* createLink(Plug* fromPlug, Plug* toPlug);

        schema::Schema const* schema = nullptr;
        std::vector<std::unique_ptr<Node>> nodes;
        std::vector<std::unique_ptr<Link>> links;
        std::vector<std::unique_ptr<Variable>> variables;
        int nextId = 1;
    };

    Node* Graph::findNode(int nodeId)
    {
        for (auto& ptr : nodes)
            if (ptr->id == nodeId)
                return ptr.get();
        return nullptr;
    }

    Plug* Graph::findPlug(int plugId)
    {
        for (auto& nodePtr : nodes)
            for (auto& plugPtr : nodePtr->plugs)
                if (plugPtr->id == plugId)
                    return plugPtr.get();
        return nullptr;
    }

    Link* Graph::findLink(int linkId)
    {
        for (auto& linkPtr : links)
            if (linkPtr->id == linkId)
                return linkPtr.get();
        return nullptr;
    }

    void Graph::destroyNode(Node* node)
    {
        for (auto it = nodes.begin(); it != nodes.end(); ++it)
        {
            if (it->get() == node)
            {
                std::vector<Link*> linksToDelete;
                for (auto& plugPtr : node->plugs)
                    linksToDelete.insert(linksToDelete.end(), plugPtr->links.begin(), plugPtr->links.end());
                for (Link* link : linksToDelete)
                    destroyLink(link);
                nodes.erase(it);
                return;
            }
        }
    }

    void Graph::destroyLink(Link* link)
    {
        for (auto it = links.begin(); it != links.end(); ++it)
        {
            if (it->get() == link)
            {
                std::erase(link->fromPlug->links, link);
                std::erase(link->toPlug->links, link);
                links.erase(it);
                return;
            }
        }
    }

    Node* Graph::createNode(std::string_view name)
    {
        schema::Node const* nodeSchema = schema->findNode(name);
        if (nodeSchema == nullptr)
            return nullptr;

        auto& nodePtr = nodes.emplace_back(std::make_unique<Node>());
        nodePtr->schema = nodeSchema;
        nodePtr->id = nextId++;

        for (auto const& plugSchemaPtr : nodeSchema->plugs)
        {
            auto& plugPtr = nodePtr->plugs.emplace_back(std::make_unique<Plug>());
            plugPtr->node = nodePtr.get();
            plugPtr->schema = plugSchemaPtr.get();
            plugPtr->id = nextId++;
        }

        for (auto const& slotSchemaPtr : nodeSchema->slots)
        {
            auto& slotPtr = nodePtr->slots.emplace_back(std::make_unique<Slot>());
            slotPtr->node = nodePtr.get();
            slotPtr->schema = slotSchemaPtr.get();
        }

        return nodePtr.get();
    }

    Link* Graph::createLink(Plug* fromPlug, Plug* toPlug)
    {
        if (fromPlug == nullptr || toPlug == nullptr)
            return nullptr;

        if (schema::IsInputPlug(fromPlug->schema->kind))
            return nullptr;
        if (!schema::IsInputPlug(toPlug->schema->kind))
            return nullptr;

        auto& linkPtr = links.emplace_back(std::make_unique<Link>());
        linkPtr->fromPlug = fromPlug;
        linkPtr->toPlug = toPlug;
        linkPtr->id = nextId++;

        fromPlug->links.push_back(linkPtr.get());
        toPlug->links.push_back(linkPtr.get());

        return linkPtr.get();
    }

} // namespace sample::graph
