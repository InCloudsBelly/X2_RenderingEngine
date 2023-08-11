#include "Precompiled.h"
#include "Base.h"

#include "Log.h"
#include "Memory.h"

#define X2_BUILD_ID "v0.1a"

namespace X2 {

	void InitializeCore()
	{
		Allocator::Init();
		Log::Init();

		X2_CORE_TRACE_TAG("Core", "X2 Engine {}", X2_BUILD_ID);
		X2_CORE_TRACE_TAG("Core", "Initializing...");
	}

	void ShutdownCore()
	{
		X2_CORE_TRACE_TAG("Core", "Shutting down...");

		Log::Shutdown();
	}

}
