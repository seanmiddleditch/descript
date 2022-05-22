// descript

#pragma once

#include <cstdint>

namespace descript {
    enum class dsOpCode : uint8_t
    {
        Nop = 0,

        // fast op-codes for pushing constant values
        PushTrue,
        PushFalse,
        PushNil,
        Push0,
        Push1,
        Push2,
        PushNeg1,

        // fast op-codes for pushing immediates
        PushS8,
        PushU8,
        PushS16,
        PushU16,

        // generic constant push
        PushConstant,

        // variable access
        Read,

        // function access
        Call,

        // unary operators
        Neg,
        Not,

        // binary operators
        Add,
        Sub,
        Mul,
        Div,
        And,
        Or,
        Xor,

        Last,
    };
} // namespace descript
