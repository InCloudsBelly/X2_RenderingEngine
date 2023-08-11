#pragma once

#include "X2/Core/Base.h"
#include "X2/Core/Timestep.h"
#include "X2/Core/Timer.h"
#include "X2/Core/Event/ApplicationEvent.h"
#include "X2/Core/WindowsWindow.h"
#include "X2/Core/LayerStack.h"
//#include "X2/Script/ScriptEngine.h"
#include "X2/Renderer/RendererConfig.h"

#include "X2/Core/Event/ApplicationEvent.h"
#include "X2/Core/RenderThread.h"

#include "X2/ImGui/ImGuiLayer.h"
#include <queue>

#include <queue>

namespace X2 {

	struct ApplicationSpecification
	{
		std::string Name = "X2";
		uint32_t WindowWidth = 1920, WindowHeight = 1080;
		bool WindowDecorated = false;
		bool Fullscreen = false;
		bool VSync = true;
		std::string WorkingDirectory;
		bool StartMaximized = true;
		bool Resizable = true;
		bool EnableImGui = true;
		//ScriptEngineConfig ScriptConfig;
		RendererConfig RenderConfig;
		ThreadingPolicy CoreThreadingPolicy = ThreadingPolicy::MultiThreaded;
	};

	class Application
	{
		using EventCallbackFn = std::function<void(Event&)>;
	public:
		struct PerformanceTimers
		{
			float MainThreadWorkTime = 0.0f;
			float MainThreadWaitTime = 0.0f;
			float RenderThreadWorkTime = 0.0f;
			float RenderThreadWaitTime = 0.0f;
		};
	public:
		Application(const ApplicationSpecification& specification);
		virtual ~Application();

		void Run();
		void Close();

		virtual void OnInit() {}
		virtual void OnShutdown();
		virtual void OnUpdate(Timestep ts) {}

		virtual void OnEvent(Event& event);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* layer);
		void RenderImGui();

		void AddEventCallback(const EventCallbackFn& eventCallback) { m_EventCallbacks.push_back(eventCallback); }

		void SetShowStats(bool show) { m_ShowStats = show; }

		template<typename Func>
		void QueueEvent(Func&& func)
		{
			m_EventQueue.push(func);
		}

		/// Creates & Dispatches an event either immediately, or adds it to an event queue which will be proccessed at the end of each frame
		template<typename TEvent, bool DispatchImmediately = false, typename... TEventArgs>
		void DispatchEvent(TEventArgs&&... args)
		{
			static_assert(std::is_assignable_v<Event, TEvent>);

			std::shared_ptr<TEvent> event = std::make_shared<TEvent>(std::forward<TEventArgs>(args)...);
			if constexpr (DispatchImmediately)
			{
				OnEvent(*event);
			}
			else
			{
				std::scoped_lock<std::mutex> lock(m_EventQueueMutex);
				m_EventQueue.push([event]() { Application::Get().OnEvent(*event); });
			}
		}

		inline WindowsWindow& GetWindow() { return *m_Window; }

		static inline Application& Get() { return *s_Instance; }

		Timestep GetTimestep() const { return m_TimeStep; }
		Timestep GetFrametime() const { return m_Frametime; }
		float GetTime() const; // TODO: This should be in "Platform"

		static const char* GetConfigurationName();
		static const char* GetPlatformName();

		const ApplicationSpecification& GetSpecification() const { return m_Specification; }

		PerformanceProfiler* GetPerformanceProfiler() { return m_Profiler; }

		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		RenderThread& GetRenderThread() { return m_RenderThread; }
		uint32_t GetCurrentFrameIndex() const { return m_CurrentFrameIndex; }
		PerformanceTimers GetPerformanceTimers() const { return m_PerformanceTimers; }
		const std::unordered_map<const char*, float>& GetProfilerPreviousFrameData() const { return m_ProfilerPreviousFrameData; }

		static bool IsRuntime() { return s_IsRuntime; }
	private:
		void ProcessEvents();

		bool OnWindowResize(WindowResizeEvent& e);
		bool OnWindowMinimize(WindowMinimizeEvent& e);
		bool OnWindowClose(WindowCloseEvent& e);
	private:
		std::unique_ptr<WindowsWindow> m_Window;
		ApplicationSpecification m_Specification;
		bool m_Running = true, m_Minimized = false;
		LayerStack m_LayerStack;

		ImGuiLayer* m_ImGuiLayer;
		
		Timestep m_Frametime;
		Timestep m_TimeStep;
		PerformanceProfiler* m_Profiler = nullptr; // TODO: Should be null in Dist
		std::unordered_map<const char*, float> m_ProfilerPreviousFrameData;
		bool m_ShowStats = true;

		RenderThread m_RenderThread;

		std::mutex m_EventQueueMutex;
		std::queue<std::function<void()>> m_EventQueue;
		std::vector<EventCallbackFn> m_EventCallbacks;

		float m_LastFrameTime = 0.0f;
		uint32_t m_CurrentFrameIndex = 0;

		PerformanceTimers m_PerformanceTimers; // TODO(Yan): remove for Dist

		static Application* s_Instance;

		friend class Renderer;
	protected:
		inline static bool s_IsRuntime = false;
	};

	// Implemented by CLIENT
	Application* CreateApplication(int argc, char** argv);
}
