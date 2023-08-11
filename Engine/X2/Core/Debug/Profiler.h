#pragma once

#define X2_ENABLE_PROFILING !X2_DIST

#if X2_ENABLE_PROFILING 
#include <optick.h>
#endif

#if X2_ENABLE_PROFILING
#define X2_PROFILE_FRAME(...)           OPTICK_FRAME(__VA_ARGS__)
#define X2_PROFILE_FUNC(...)            OPTICK_EVENT(__VA_ARGS__)
#define X2_PROFILE_TAG(NAME, ...)       OPTICK_TAG(NAME, __VA_ARGS__)
#define X2_PROFILE_SCOPE_DYNAMIC(NAME)  OPTICK_EVENT_DYNAMIC(NAME)
#define X2_PROFILE_THREAD(...)          OPTICK_THREAD(__VA_ARGS__)
#else
#define X2_PROFILE_FRAME(...)
#define X2_PROFILE_FUNC(...)
#define X2_PROFILE_TAG(NAME, ...) 
#define X2_PROFILE_SCOPE_DYNAMIC(NAME)
#define X2_PROFILE_THREAD(...)
#endif
