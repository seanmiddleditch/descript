#pragma once

#include <descript/types.hh>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sample::schema {
    struct Node;
    struct Plug;
    struct Slot;

    enum class NodeKind
    {
        Entry,
        State
    };

    enum class PlugKind
    {
        Begin,
        Default,
        Input,
        Output,
    };

    constexpr bool IsInputPlug(PlugKind kind) noexcept { return kind == PlugKind::Begin || kind == PlugKind::Input; }

    enum class SlotKind
    {
        Input,
        Output,
        Pair,
    };

    struct Node
    {
        std::string name;
        std::string title;
        std::vector<std::unique_ptr<Plug>> plugs;
        std::vector<std::unique_ptr<Slot>> slots;
        NodeKind kind = NodeKind::State;
        int typeId = -1;
    };

    struct Plug
    {
        Node* node = nullptr;
        std::string name;
        PlugKind kind = PlugKind::Default;
        uint8_t index = 0;
    };

    struct Slot
    {
        Node* node = nullptr;
        std::string name;
        SlotKind kind = SlotKind::Input;
        uint8_t index = 0;
    };

    struct Schema
    {
        inline Node const* findNode(std::string_view name) const;

        inline Node& createNode(std::string name, NodeKind kind);
        inline Plug& createPlug(Node& node, std::string name, PlugKind kind, uint8_t index);
        inline Slot& createSlot(Node& node, std::string name, SlotKind kind, uint8_t index);

        std::vector<std::unique_ptr<Node>> nodes;
        int nextNodeTypeId = 1;
    };

    Node const* Schema::findNode(std::string_view name) const
    {
        for (auto& ptr : nodes)
            if (ptr->name == name)
                return ptr.get();
        return nullptr;
    }

    Node& Schema::createNode(std::string name, NodeKind kind)
    {
        auto& ptr = nodes.emplace_back(std::make_unique<Node>());
        ptr->name = name;
        ptr->kind = kind;
        ptr->typeId = nextNodeTypeId++;
        return *ptr;
    }

    Plug& Schema::createPlug(Node& node, std::string name, PlugKind kind, uint8_t index)
    {
        auto& ptr = node.plugs.emplace_back(std::make_unique<Plug>());
        ptr->node = &node;
        ptr->name = name;
        ptr->kind = kind;
        ptr->index = index;
        return *ptr;
    }

    Slot& Schema::createSlot(Node& node, std::string name, SlotKind kind, uint8_t index)
    {
        auto& ptr = node.slots.emplace_back(std::make_unique<Slot>());
        ptr->node = &node;
        ptr->name = name;
        ptr->kind = kind;
        ptr->index = index;
        return *ptr;
    }
} // namespace sample::schema
