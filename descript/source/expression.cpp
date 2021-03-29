// descript

#include "descript/value.hh"

#include "array.hh"
#include "assembly_internal.hh"
#include "expression.hh"
#include "ops.hh"
#include "utility.hh"

#include <utility>

namespace descript {
    namespace {
        class Context final : public dsFunctionContext
        {
        public:
            Context(dsEvaluateHost& host, uint32_t argc, dsValue const* argv) noexcept : host_(host), argc_(argc), argv_(argv) {}

            uint32_t argc() const noexcept { return argc_; }
            dsValue const& argAt(uint32_t index) const noexcept
            {
                DS_GUARD_OR(index < argc_, default_);
                return argv_[index];
            }

            void listen(dsEmitterId emitterId) override { host_.listen(emitterId); }

        private:
            dsEvaluateHost& host_;
            uint32_t argc_ = 0;
            dsValue const* argv_ = nullptr;
            dsValue default_;
        };

        struct Neg
        {
            static constexpr double apply(double val) noexcept { return -val; }
        };

        struct Not
        {
            static constexpr double apply(double val) noexcept { return !val; }
        };

        struct Add
        {
            static constexpr double apply(double left, double right) noexcept { return left + right; }
        };

        struct Sub
        {
            static constexpr double apply(double left, double right) noexcept { return left - right; }
        };

        struct Mul
        {
            static constexpr double apply(double left, double right) noexcept { return left * right; }
        };

        struct Div
        {
            static constexpr double apply(double left, double right) noexcept { return left / right; }
        };

        template <typename Op>
        constexpr dsValue apply(dsValue const& val) noexcept
        {
            switch (val.type())
            {
            case dsValueType::Double: return dsValue{Op::apply(val.as<double>())};
            default: return dsValue{};
            }
        }

        // note: left/right are swapped because we pop the right before the left
        template <typename Op>
        constexpr dsValue apply(dsValue const& left, dsValue const& right) noexcept
        {
            if (left.type() != right.type())
                return dsValue{};

            switch (left.type())
            {
            case dsValueType::Double: return dsValue{Op::apply(left.as<double>(), right.as<double>())};
            default: return dsValue{};
            }
        }
    } // namespace

#define DS_CHECKTOP()          \
    if (stackTop >= stackSize) \
        return false;          \
    else                       \
        ;

#define DS_PUSH(val)               \
    if (true)                      \
    {                              \
        DS_CHECKTOP()              \
        stack[stackTop++] = (val); \
    }                              \
    else                           \
        ;

#define DS_POP() (stackTop > 0 ? std::move(stack[--stackTop]) : dsValue{})

#define DS_BINOP(op)                     \
    {                                    \
        auto const& right = DS_POP();    \
        auto const& left = DS_POP();     \
        DS_PUSH(apply<op>(left, right)); \
    }

    bool dsEvaluate(dsEvaluateHost& host, uint8_t const* ops, uint32_t opsLen, dsValue& out_value)
    {
        DS_GUARD_OR(ops != nullptr, false);
        DS_GUARD_OR(opsLen > 0, false);

        uint8_t const* const opsEnd = ops + opsLen;

        constexpr uint32_t stackSize = 16;
        dsValue stack[stackSize];
        uint32_t stackTop = 0;

        for (uint8_t const* ip = ops; ip != opsEnd; ++ip)
        {
            switch (dsOpCode(*ip))
            {
            case dsOpCode::Nop: break;
            case dsOpCode::Push0: DS_PUSH(dsValue{0.0}); break;
            case dsOpCode::Push1: DS_PUSH(dsValue{1.0}); break;
            case dsOpCode::PushNeg1: DS_PUSH(dsValue{-1.0}); break;
            case dsOpCode::PushConstant: {
                if (++ip == opsEnd)
                    return false;
                uint16_t index = *ip << 8;
                if (++ip == opsEnd)
                    return false;
                index |= *ip;
                DS_CHECKTOP();
                if (!host.readConstant(dsExpressionConstantIndex{index}, stack[stackTop]))
                    return false;
                ++stackTop;
                break;
            }
            case dsOpCode::Read: {
                if (++ip == opsEnd)
                    return false;
                uint16_t index = *ip << 8;
                if (++ip == opsEnd)
                    return false;
                index |= *ip;
                DS_CHECKTOP();
                if (!host.readVariable(dsExpressionVariableIndex{index}, stack[stackTop]))
                    return false;
                ++stackTop;
                break;
            }
            case dsOpCode::Call: {
                if (++ip == opsEnd)
                    return false;
                uint16_t index = *ip << 8;
                if (++ip == opsEnd)
                    return false;
                index |= *ip;
                if (++ip == opsEnd)
                    return false;
                uint8_t argc = *ip;
                if (argc > stackTop)
                    return false;

                dsValue result;
                Context ctx(host, argc, stack + (stackTop - argc));
                if (!host.invokeFunction(dsExpressionFunctionIndex{index}, ctx, result))
                    return false;

                stackTop -= argc;
                DS_PUSH(std::move(result));
                break;
            }
            case dsOpCode::Neg: DS_PUSH(apply<Neg>(DS_POP())); break;
            case dsOpCode::Not: DS_PUSH(apply<Not>(DS_POP())); break;
            case dsOpCode::Add: DS_BINOP(Add); break;
            case dsOpCode::Sub: DS_BINOP(Sub); break;
            case dsOpCode::Mul: DS_BINOP(Mul); break;
            case dsOpCode::Div: DS_BINOP(Div); break;
            default: return false;
            }
        }

        if (stackTop != 1)
            return false;

        out_value = DS_POP();
        return true;
    }
} // namespace descript
