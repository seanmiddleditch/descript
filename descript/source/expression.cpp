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

        template <typename T>
        constexpr bool IsArithmetic =
            std::conjunction_v<std::is_scalar<T>, std::negation<std::is_enum<T>>, std::negation<std::is_same<T, bool>>>;

        static_assert(IsArithmetic<int>);
        static_assert(IsArithmetic<float>);
        static_assert(!IsArithmetic<bool>);
        static_assert(!IsArithmetic<dsValueType>);

        struct Neg
        {
            template <typename T>
            requires IsArithmetic<T>
            static constexpr T apply(T val) noexcept { return -val; }
        };

        struct Not
        {
            static constexpr bool apply(bool val) noexcept { return !val; }
        };

        struct And
        {
            static constexpr bool apply(bool left, bool right) noexcept { return left && right; }
        };

        struct Or
        {
            static constexpr bool apply(bool left, bool right) noexcept { return left || right; }
        };

        struct Xor
        {
            static constexpr bool apply(bool left, bool right) noexcept { return left ^ right; }
        };

        struct Add
        {
            template <typename T>
            requires IsArithmetic<T>
            static constexpr T apply(T left, T right) noexcept { return left + right; }
        };

        struct Sub
        {
            template <typename T>
            requires IsArithmetic<T>
            static constexpr T apply(T left, T right) noexcept { return left - right; }
        };

        struct Mul
        {
            template <typename T>
            requires IsArithmetic<T>
            static constexpr T apply(T left, T right) noexcept { return left * right; }
        };

        struct Div
        {
            template <typename T>
            requires IsArithmetic<T>
            static constexpr T apply(T left, T right) noexcept { return right != T{0} ? left / right : T{0}; }
        };

        template <typename Op, typename... T>
        constexpr bool IsApplicable = requires(T const&... args)
        {
            Op::apply(args...);
        };

        static_assert(IsApplicable<Add, int, int>);

        template <typename Op, typename T>
        constexpr dsValue apply(T const& val) noexcept
        {
            if constexpr (IsApplicable<Op, T>)
                return dsValue{Op::apply(val)};
            else
                return nullptr;
        }

        template <typename Op>
        constexpr dsValue apply(dsValue const& val) noexcept
        {
            switch (val.type())
            {
            case dsValueType::Int32: return apply<Op>(val.as<int32_t>());
            case dsValueType::Float32: return apply<Op>(val.as<float>());
            case dsValueType::Bool: return apply<Op>(val.as<bool>());
            default: return dsValue{};
            }
        }

        template <typename Op, typename T, typename U>
        constexpr dsValue apply(T const& left, U const& right) noexcept
        {
            if constexpr (IsApplicable<Op, T, U>)
                return dsValue{Op::apply(left, right)};
            else
                return nullptr;
        }

        // note: left/right are swapped because we pop the right before the left
        template <typename Op>
        constexpr dsValue apply(dsValue const& left, dsValue const& right) noexcept
        {
            if (left.type() != right.type())
                return dsValue{};

            switch (left.type())
            {
            case dsValueType::Int32: return apply<Op>(left.as<int32_t>(), right.as<int32_t>());
            case dsValueType::Float32: return apply<Op>(left.as<float>(), right.as<float>());
            case dsValueType::Bool: return apply<Op>(left.as<bool>(), right.as<bool>());
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

        constexpr uint32_t stackSize = 32;
        dsValue stack[stackSize];
        uint32_t stackTop = 0;

        for (uint8_t const* ip = ops; ip != opsEnd; ++ip)
        {
            switch (dsOpCode(*ip))
            {
            case dsOpCode::Nop: break;
            case dsOpCode::PushTrue: DS_PUSH(dsValue{true}); break;
            case dsOpCode::PushFalse: DS_PUSH(dsValue{false}); break;
            case dsOpCode::PushNil: DS_PUSH(dsValue{nullptr}); break;
            case dsOpCode::Push0: DS_PUSH(dsValue{0}); break;
            case dsOpCode::Push1: DS_PUSH(dsValue{1}); break;
            case dsOpCode::Push2: DS_PUSH(dsValue{2}); break;
            case dsOpCode::PushNeg1: DS_PUSH(dsValue{-1}); break;
            case dsOpCode::PushS8:
                if (++ip == opsEnd)
                    return false;
                DS_PUSH(dsValue{static_cast<int32_t>(static_cast<int8_t>(*ip))});
                break;
            case dsOpCode::PushU8:
                if (++ip == opsEnd)
                    return false;
                DS_PUSH(dsValue{static_cast<int32_t>(*ip)});
                break;
            case dsOpCode::PushS16: {
                if (++ip == opsEnd)
                    return false;
                uint16_t value = *ip << 8;
                if (++ip == opsEnd)
                    return false;
                value |= *ip;
                DS_PUSH(dsValue{static_cast<int32_t>(static_cast<int16_t>(value))});
                break;
            }
            case dsOpCode::PushU16: {
                if (++ip == opsEnd)
                    return false;
                uint16_t value = *ip << 8;
                if (++ip == opsEnd)
                    return false;
                value |= *ip;
                DS_PUSH(dsValue{static_cast<int32_t>(value)});
                break;
            }
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
            case dsOpCode::And: DS_BINOP(And); break;
            case dsOpCode::Or: DS_BINOP(Or); break;
            case dsOpCode::Xor: DS_BINOP(Xor); break;
            default: return false;
            }
        }

        if (stackTop != 1)
            return false;

        out_value = DS_POP();
        return true;
    }
} // namespace descript
