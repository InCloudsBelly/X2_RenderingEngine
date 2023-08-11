#include "Precompiled.h"
#include "Thread.h"

#include <Windows.h>

namespace X2 {

	Thread::Thread(const std::string& name)
		: m_Name(name)
	{
	}

	void Thread::SetName(const std::string& name)
	{
		HANDLE threadHandle = m_Thread.native_handle();

		std::wstring wName(name.begin(), name.end());
		SetThreadDescription(threadHandle, wName.c_str());
		SetThreadAffinityMask(threadHandle, 8);
	}

	ThreadSignal::ThreadSignal(const std::string& name, bool manualReset)
	{
		std::string str(name.begin(), name.end());
		m_SignalHandle = CreateEvent(NULL, (BOOL)manualReset, FALSE, str.c_str());
	}

	void ThreadSignal::Wait()
	{
		WaitForSingleObject(m_SignalHandle, INFINITE);
	}

	void Thread::Join()
	{
		m_Thread.join();
	}

	void ThreadSignal::Signal()
	{
		SetEvent(m_SignalHandle);
	}

	void ThreadSignal::Reset()
	{
		ResetEvent(m_SignalHandle);
	}

}
