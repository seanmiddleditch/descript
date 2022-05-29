// descript

#pragma once

#if defined(DS_EXPORT)
#if defined(_WINDOWS)
#define DS_API __declspec(dllexport)
#else
#define DS_API [[gnu::visibility("default")]]
#endif
#else
#define DS_API
#endif

#if defined(DS_EXTRA_EXPORT)
#if defined(_WINDOWS)
#define DS_EXTRA_API __declspec(dllexport)
#else
#define DS_EXTRA_API [[gnu::visibility("default")]]
#endif
#else
#define DS_EXTRA_API
#endif
