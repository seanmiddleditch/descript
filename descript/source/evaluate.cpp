// descript

#include "descript/evaluate.hh"

#include "descript/context.hh"
#include "descript/value.hh"

#include "array.hh"
#include "assembly_internal.hh"
#include "ops.hh"
#include "utility.hh"

#include <utility>

namespace descript {
    namespace {
        static constexpr uint32_t s_stackSize = 32;

        class Context final : public dsFunctionContext
        {
        public:
            Context(dsEvaluateHost& host, uint32_t argc, dsValueStorage const* argv, dsValueStorage& result) noexcept
                : host_(host), argc_(argc), argv_(argv), result_(result)
            {
            }

            uint32_t getArgCount() const noexcept { return argc_; }
            dsValueRef getArgValueAt(uint32_t index) const noexcept
            {
                DS_ASSERT(index < argc_);
                return argv_[index].ref();
            }

            void result(dsValueRef const& result) override { result_ = dsValueStorage{result}; }

            void listen(dsEmitterId emitterId) override { host_.listen(emitterId); }

        private:
            dsEvaluateHost& host_;
            uint32_t argc_ = 0;
            dsValueStorage const* argv_ = nullptr;
            dsValueStorage& result_;
        };

        template <typename T>
        constexpr bool IsArithmetic =
            std::conjunction_v<std::is_arithmetic<T>, std::negation<std::is_same<T, bool>>>;

        static_assert(IsArithmetic<int>);
        static_assert(IsArithmetic<float>);
        static_assert(!IsArithmetic<bool>);
        static_assert(!IsArithmetic<char*>);
        static_assert(!IsArithmetic<dsPlugKind>);

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
        constexpr bool applyTyped(dsValueOut& out_result, T const& val) noexcept
        {
            if constexpr (IsApplicable<Op, T>)
                return out_result.accept(Op::apply(val));
            else
                return false;
        }

        template <typename Op>
        constexpr bool apply(dsValueOut out_result, dsValueStorage const& val) noexcept
        {
            if (val.type() == dsType<int32_t>.typeId)
                return applyTyped<Op>(out_result, val.as<int32_t>());
            if (val.type() == dsType<float>.typeId)
                return applyTyped<Op>(out_result, val.as<float>());
            if (val.type() == dsType<bool>.typeId)
                return applyTyped<Op>(out_result, val.as<bool>());
            return false;
        }

        template <typename Op, typename T, typename U>
        constexpr bool applyTyped(dsValueOut& out_result, T const& left, U const& right) noexcept
        {
            if constexpr (IsApplicable<Op, T, U>)
                return out_result.accept(Op::apply(left, right));
            else
                return false;
        }

        // note: left/right are swapped because we pop the right before the left
        template <typename Op>
        constexpr bool apply(dsValueOut out_result, dsValueStorage const& left, dsValueStorage const& right) noexcept
        {
            if (left.type() != right.type())
                return false;

            if (left.type() == dsType<int32_t>.typeId)
                return applyTyped<Op>(out_result, left.as<int32_t>(), right.as<int32_t>());
            if (left.type() == dsType<float>.typeId)
                return applyTyped<Op>(out_result, left.as<float>(), right.as<float>());
            if (left.type() == dsType<bool>.typeId)
                return applyTyped<Op>(out_result, left.as<bool>(), right.as<bool>());
            return false;
        }

        union RefUnion {
            char dummy_ = 0;
            dsValueRef ref;

            RefUnion() = default;
            ~RefUnion() = default;
        };
    } // namespace

#define DS_CHECKTOP()            \
    if (stackTop >= s_stackSize) \
        return false;            \
    else                         \
        ;

#define DS_PUSH(val)                             \
    if (true)                                    \
    {                                            \
        DS_CHECKTOP()                            \
        stack[stackTop++] = dsValueStorage(val); \
    }                                            \
    else                                         \
        ;

#define DS_POP(result) \
    if (stackTop == 0) \
        return false;  \
    result = std::move(stack[--stackTop])

#define DS_UNNOP(op)                                  \
    {                                                 \
        DS_POP(auto const& value);                    \
        if (!apply<op>(stack[stackTop].out(), value)) \
            return false;                             \
        ++stackTop;                                   \
    }

#define DS_BINOP(op)                                        \
    {                                                       \
        DS_POP(auto const& right);                          \
        DS_POP(auto const& left);                           \
        if (!apply<op>(stack[stackTop].out(), left, right)) \
            return false;                                   \
        ++stackTop;                                         \
    }

    bool dsEvaluate(dsEvaluateHost& host, uint8_t const* ops, uint32_t opsLen, dsValueOut out_value)
    {
        DS_GUARD_OR(ops != nullptr, false);
        DS_GUARD_OR(opsLen > 0, false);

        uint8_t const* const opsEnd = ops + opsLen;

        dsValueStorage stack[s_stackSize];
        uint32_t stackTop = 0;

        for (uint8_t const* ip = ops; ip != opsEnd; ++ip)
        {
            switch (dsOpCode(*ip))
            {
            case dsOpCode::Nop: break;
            case dsOpCode::PushTrue: DS_PUSH(true); break;
            case dsOpCode::PushFalse: DS_PUSH(false); break;
            case dsOpCode::PushNil: DS_PUSH(nullptr); break;
            case dsOpCode::Push0: DS_PUSH(0); break;
            case dsOpCode::Push1: DS_PUSH(1); break;
            case dsOpCode::Push2: DS_PUSH(2); break;
            case dsOpCode::PushNeg1: DS_PUSH(-1); break;
            case dsOpCode::PushS8:
                if (++ip == opsEnd)
                    return false;
                DS_PUSH(static_cast<int32_t>(static_cast<int8_t>(*ip)));
                break;
            case dsOpCode::PushU8:
                if (++ip == opsEnd)
                    return false;
                DS_PUSH(static_cast<int32_t>(*ip));
                break;
            case dsOpCode::PushS16: {
                if (++ip == opsEnd)
                    return false;
                uint16_t value = *ip << 8;
                if (++ip == opsEnd)
                    return false;
                value |= *ip;
                DS_PUSH(static_cast<int32_t>(static_cast<int16_t>(value)));
                break;
            }
            case dsOpCode::PushU16: {
                if (++ip == opsEnd)
                    return false;
                uint16_t value = *ip << 8;
                if (++ip == opsEnd)
                    return false;
                value |= *ip;
                DS_PUSH(static_cast<int32_t>(value));
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
                if (!host.readConstant(index, stack[stackTop].out()))
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
                if (!host.readVariable(index, stack[stackTop].out()))
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

                dsValueStorage result;
                uint32_t const stackArgOffset = stackTop - argc;
                Context ctx(host, argc, &stack[stackArgOffset], result);
                if (!host.invokeFunction(index, ctx))
                    return false;

                stackTop -= argc;
                DS_PUSH(std::move(result));
                break;
            }
            case dsOpCode::Neg: DS_UNNOP(Neg); break;
            case dsOpCode::Not: DS_UNNOP(Not); break;
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

        return out_value.accept(stack[0].ref());
    }
} // namespace descript
