#include "Precompiled.h"
#include "RenderThread.h"

#include <Windows.h>

#include "X2/Renderer/Renderer.h"


namespace X2 {

	struct RenderThreadData
	{
		CRITICAL_SECTION m_CriticalSection;
		CONDITION_VARIABLE m_ConditionVariable;

		RenderThread::State m_State = RenderThread::State::Idle;
	};

	RenderThread::RenderThread(ThreadingPolicy coreThreadingPolicy)
		: m_RenderThread("Render Thread"), m_ThreadingPolicy(coreThreadingPolicy)
	{
		m_Data = new RenderThreadData();

		if (m_ThreadingPolicy == ThreadingPolicy::MultiThreaded)
		{
			InitializeCriticalSection(&m_Data->m_CriticalSection);
			InitializeConditionVariable(&m_Data->m_ConditionVariable);
		}
	}

	RenderThread::~RenderThread()
	{
		if (m_ThreadingPolicy == ThreadingPolicy::MultiThreaded)
			DeleteCriticalSection(&m_Data->m_CriticalSection);
	}

	void RenderThread::Run()
	{
		m_IsRunning = true;
		if (m_ThreadingPolicy == ThreadingPolicy::MultiThreaded)
			m_RenderThread.Dispatch(Renderer::RenderThreadFunc, this);
	}

	void RenderThread::Terminate()
	{
		m_IsRunning = false;
		Pump();

		if (m_ThreadingPolicy == ThreadingPolicy::MultiThreaded)
			m_RenderThread.Join();
	}

	void RenderThread::Wait(State waitForState)
	{
		if (m_ThreadingPolicy == ThreadingPolicy::SingleThreaded)
			return;

		EnterCriticalSection(&m_Data->m_CriticalSection);
		while (m_Data->m_State != waitForState)
		{
			// This releases the CS so that another thread can wake it
			SleepConditionVariableCS(&m_Data->m_ConditionVariable, &m_Data->m_CriticalSection, INFINITE);
		}
		LeaveCriticalSection(&m_Data->m_CriticalSection);
	}

	void RenderThread::WaitAndSet(State waitForState, State setToState)
	{
		if (m_ThreadingPolicy == ThreadingPolicy::SingleThreaded)
			return;

		EnterCriticalSection(&m_Data->m_CriticalSection);
		while (m_Data->m_State != waitForState)
		{
			SleepConditionVariableCS(&m_Data->m_ConditionVariable, &m_Data->m_CriticalSection, INFINITE);
		}
		m_Data->m_State = setToState;
		WakeAllConditionVariable(&m_Data->m_ConditionVariable);
		LeaveCriticalSection(&m_Data->m_CriticalSection);
	}

	void RenderThread::Set(State setToState)
	{
		if (m_ThreadingPolicy == ThreadingPolicy::SingleThreaded)
			return;

		EnterCriticalSection(&m_Data->m_CriticalSection);
		m_Data->m_State = setToState;
		WakeAllConditionVariable(&m_Data->m_ConditionVariable);
		LeaveCriticalSection(&m_Data->m_CriticalSection);
	}

	void RenderThread::NextFrame()
	{
		m_AppThreadFrame++;
		Renderer::SwapQueues();
	}

	void RenderThread::BlockUntilRenderComplete()
	{
		if (m_ThreadingPolicy == ThreadingPolicy::SingleThreaded)
			return;

		Wait(State::Idle);
	}

	void RenderThread::Kick()
	{
		if (m_ThreadingPolicy == ThreadingPolicy::MultiThreaded)
		{
			Set(State::Kick);
		}
		else
		{
			Renderer::WaitAndRender(this);
		}
	}

	void RenderThread::Pump()
	{
		NextFrame();
		Kick();
		BlockUntilRenderComplete();
	}

}
