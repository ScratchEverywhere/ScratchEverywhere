#pragma once

#ifdef _WIN32
#ifdef SE_BUILD_LIBRARY
#define SE_EXPORT __declspec(dllexport)
#elif defined(SE_DLL)
#define SE_EXPORT __declspec(dllimport)
#else
#define SE_EXPORT
#endif
#else
#define SE_EXPORT __attribute__((visibility("default")))
#endif
