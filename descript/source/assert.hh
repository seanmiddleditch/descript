// descript

#pragma once

#if !defined(DS_ASSERT)
#include <cassert>
#define DS_ASSERT(x, ...) assert(x)
#endif

#if _MSC_VER
#define DS_BREAK() __debugbreak()
#else
#define DS_BREAK() (void)0
#endif

#if !defined(DS_VERIFY)
#define DS_VERIFY(x, ...) !!((x) || (DS_BREAK(), false))
#endif

#define DS_GUARD_OR(x, r, ...) \
    if (DS_VERIFY(x))          \
    {                          \
    }                          \
    else                       \
    {                          \
        DS_BREAK();            \
        return (r);            \
    }

#define DS_GUARD_VOID(x, ...) \
    if (DS_VERIFY(x))         \
    {                         \
    }                         \
    else                      \
    {                         \
        DS_BREAK();           \
        return;               \
    }
