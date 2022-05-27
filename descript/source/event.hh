// descript

#pragma once

#include "descript/types.hh"

namespace descript {
    struct dsEvent final
    {
        union {
            char _unused = 0;
            struct Input
            {
                dsInputPlugIndex inputPlugIndex;
            } input;
        } data = {};
        dsEventType type = dsEventType::Activate;
    };
} // namespace descript
