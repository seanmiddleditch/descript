// descript

#pragma once

namespace descript {
    template <typename NodeT>
    class NodeBase
    {
    public:
        static void dispatch(dsNodeContext& ctx, dsEventType eventType, void* userData)
        {
            NodeT* const self = static_cast<NodeT*>(userData);

            switch (eventType)
            {
            case dsEventType::Activate:
                NodeT::construct(self);
                self->onActivate(ctx);
                break;
            case dsEventType::CustomInput: self->onCustomInput(ctx); break;
            case dsEventType::Dependency: self->onDependency(ctx); break;
            case dsEventType::Deactivate:
                self->onDeactivate(ctx);
                NodeT::destruct(self);
                break;
            }
        }

    protected:
        ~NodeBase() = default;

        // these can optionally be implemented in the derived type
        static void construct(void* memory) { new (memory) NodeT(); }
        static void destruct(NodeT* self) { self->~NodeT(); }

        // this must be implemented in the derived type
        void onActivate(dsNodeContext& ctx) = delete;

        // these can optionally be implemented in the derived type
        void onCustomInput(dsNodeContext& ctx) {}
        void onDeactivate(dsNodeContext& ctx) {}
        void onDependency(dsNodeContext& ctx) {}
    };

    template <typename NodeT>
    class NodeVirtualBase : public NodeBase<NodeVirtualBase<NodeT>>
    {
    protected:
        ~NodeVirtualBase() = default;

        static void construct(void* memory) { new (memory) NodeT(); }
        static void destruct(NodeVirtualBase* self) { static_cast<NodeT*>(self)->~NodeT(); }

        virtual void onActivate(dsNodeContext& ctx) = 0;
        virtual void onCustomInput(dsNodeContext& ctx) {}
        virtual void onDeactivate(dsNodeContext& ctx) {}
        virtual void onDependency(dsNodeContext& ctx) {}

        friend class NodeBase<NodeVirtualBase<NodeT>>;
    };
} // namespace descript
