#pragma once

#include "Log.h"

#define X2_PLATFORM_WINDOWS
#define X2_ENABLE_ASSERTS

#ifdef X2_PLATFORM_WINDOWS
#define X2_DEBUG_BREAK __debugbreak()
#else
#define X2_DEBUG_BREAK
#endif

#ifdef X2_DEBUG
#define X2_ENABLE_ASSERTS
#endif

#define X2_ENABLE_VERIFY

#ifdef X2_ENABLE_ASSERTS
#define X2_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::X2::Log::PrintAssertMessage(::X2::Log::Type::Core, "Assertion Failed", __VA_ARGS__)
#define X2_ASSERT_MESSAGE_INTERNAL(...)  ::X2::Log::PrintAssertMessage(::X2::Log::Type::Client, "Assertion Failed", __VA_ARGS__)

#define X2_CORE_ASSERT(condition, ...) { if(!(condition)) { X2_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); X2_DEBUG_BREAK; } }
#define X2_ASSERT(condition, ...) { if(!(condition)) { X2_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); X2_DEBUG_BREAK; } }
#else
#define X2_CORE_ASSERT(condition, ...)
#define X2_ASSERT(condition, ...)
#endif

#ifdef X2_ENABLE_VERIFY
#define X2_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::X2::Log::PrintAssertMessage(::X2::Log::Type::Core, "Verify Failed", __VA_ARGS__)
#define X2_VERIFY_MESSAGE_INTERNAL(...)  ::X2::Log::PrintAssertMessage(::X2::Log::Type::Client, "Verify Failed", __VA_ARGS__)

#define X2_CORE_VERIFY(condition, ...) { if(!(condition)) { X2_CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); X2_DEBUG_BREAK; } }
#define X2_VERIFY(condition, ...) { if(!(condition)) { X2_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); X2_DEBUG_BREAK; } }
#else
#define X2_CORE_VERIFY(condition, ...)
#define X2_VERIFY(condition, ...)
#endif
