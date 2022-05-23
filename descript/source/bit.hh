// descript

#pragma once

#include <cstring>
#include <type_traits>

namespace descript {
    template <class ResultT, class FromT>
    inline ResultT dsBitCast(FromT const& from)
    {
        static_assert(sizeof(ResultT) == sizeof(FromT));
        static_assert(std::is_trivially_constructible_v<ResultT>);
        static_assert(std::is_trivially_copyable_v<ResultT>);
        static_assert(std::is_trivially_copyable_v<FromT>);

        ResultT result;
        std::memcpy(&result, &from, sizeof(result));
        return result;
    }
}
