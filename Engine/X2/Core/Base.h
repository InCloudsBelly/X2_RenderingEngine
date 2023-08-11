#pragma once

#include <memory>
#include "Ref.h"

#define X2_PLATFORM_WINDOWS

namespace X2 {

	void InitializeCore();
	void ShutdownCore();

}

#define BIT(x) (1u << x)

#define X2_BIND_EVENT_FN(fn) std::bind(&##fn, this, std::placeholders::_1)


#ifdef X2_PLATFORM_WINDOWS
#define X2_FORCE_INLINE __forceinline
#else
// TODO: other platforms
#define X2_FORCE_INLINE inline
#endif

#include "Assert.h"

namespace X2 {

	// Pointer wrappers
	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	using byte = uint8_t;

	/** A simple wrapper for std::atomic_flag to avoid confusing
		function names usage. The object owning it can still be
		default copyable, but the copied flag is going to be reset.
	*/
	struct AtomicFlag
	{
		X2_FORCE_INLINE void SetDirty() { flag.clear(); }
		X2_FORCE_INLINE bool CheckAndResetIfDirty() { return !flag.test_and_set(); }

		explicit AtomicFlag() noexcept { flag.test_and_set(); }
		AtomicFlag(const AtomicFlag&) noexcept {}
		AtomicFlag& operator=(const AtomicFlag&) noexcept { return *this; }
		AtomicFlag(AtomicFlag&&) noexcept {};
		AtomicFlag& operator=(AtomicFlag&&) noexcept { return *this; }

	private:
		std::atomic_flag flag;
	};

	struct Flag
	{
		X2_FORCE_INLINE void SetDirty() noexcept { flag = true; }
		X2_FORCE_INLINE bool CheckAndResetIfDirty() noexcept
		{
			if (flag)
				return !(flag = !flag);
			else
				return false;
		}

		X2_FORCE_INLINE bool IsDirty() const noexcept { return flag; }

	private:
		bool flag = false;
	};

}
