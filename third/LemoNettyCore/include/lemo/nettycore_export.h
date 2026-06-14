#pragma once

#if defined(_WIN32)
#if defined(LEMO_NETTYCORE_BUILD_SHARED)
#define LEMO_NETTYCORE_API __declspec(dllexport)
#elif defined(LEMO_NETTYCORE_USE_SHARED)
#define LEMO_NETTYCORE_API __declspec(dllimport)
#else
#define LEMO_NETTYCORE_API
#endif
#else
#if defined(LEMO_NETTYCORE_BUILD_SHARED)
#define LEMO_NETTYCORE_API __attribute__((visibility("default")))
#else
#define LEMO_NETTYCORE_API
#endif
#endif
