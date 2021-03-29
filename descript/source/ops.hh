// descript

#pragma once

#include <cstdint>

namespace descript {
    enum class dsOpCode : uint8_t
    {
        Nop = 0,

        // fast op-codes for pushing common constant values
        Push0,
        Push1,
        PushNeg1,

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

        Last,
    };
} // namespace descript
