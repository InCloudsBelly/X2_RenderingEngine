#include "Precompiled.h"
#include "Application.h"

#include "X2/Renderer/Renderer.h"
#include "X2/Renderer/UI/Font.h"


#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include "X2/ImGui/Colours.h"

#include "X2/Asset/AssetManager.h"

#include "Input.h"

#include "X2/Vulkan/VulkanFramebuffer.h"
#include "X2/Vulkan/VulkanRenderer.h"
#include "X2/Vulkan/VulkanAllocator.h"
#include "X2/Vulkan/VulkanSwapChain.h"
#include "imgui/imgui_internal.h"

#include "X2/Utilities/StringUtils.h"
#include "X2/Core/Debug/Profiler.h"

#include "X2/Editor/ApplicationSettings.h"

#include <filesystem>

#include "Memory.h"

extern bool g_ApplicationRunning;
extern ImGuiContext* GImGui;

namespace X2 {

#define BIND_EVENT_FN(fn) std::bind(&Application::##fn, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification), m_RenderThread(specification.CoreThreadingPolicy)
	{
		s_Instance = this;

		m_RenderThread.Run();

		if (!specification.WorkingDirectory.empty())
			std::filesystem::current_path(specification.WorkingDirectory);

		m_Profiler = hnew PerformanceProfiler();

		Renderer::SetConfig(specification.RenderConfig);

		WindowSpecification windowSpec;
		windowSpec.Title = specification.Name;
		windowSpec.Width = specification.WindowWidth;
		windowSpec.Height = specification.WindowHeight;
		windowSpec.Decorated = specification.WindowDecorated;
		windowSpec.Fullscreen = specification.Fullscreen;
		windowSpec.VSync = specification.VSync;
		m_Window = std::unique_ptr<WindowsWindow>( new WindowsWindow(windowSpec));
		m_Window->Init();
		m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });

		// Load editor settings (will generate default settings if the file doesn't exist yet)
		ApplicationSettingsSerializer::Init();

		// Init renderer and execute command queue to compile all shaders
		Renderer::Init();
		// Render one frame (TODO: maybe make a func called Pump or something)
		m_RenderThread.Pump();

		if (specification.StartMaximized)
			m_Window->Maximize();
		else
			m_Window->CenterWindow();
		m_Window->SetResizable(specification.Resizable);

		if (m_Specification.EnableImGui)
		{
			m_ImGuiLayer = ImGuiLayer::Create();
			PushOverlay(m_ImGuiLayer);
		}

		//ScriptEngine::Init(specification.ScriptConfig);
		//MiniAudioEngine::Init();
		//Font::Init();
	}

	Application::~Application()
	{
		ApplicationSettingsSerializer::SaveSettings();

		m_Window->SetEventCallback([](Event& e) {});

		m_RenderThread.Terminate();

		for (Layer* layer : m_LayerStack)
		{
			layer->OnDetach();
			delete layer;
		}

		//ScriptEngine::Shutdown();
		Project::SetActive(nullptr);
		//Font::Shutdown();
		//MiniAudioEngine::Shutdown();

		Renderer::Shutdown();

		delete m_Profiler;
		m_Profiler = nullptr;
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::PopLayer(Layer* layer)
	{
		m_LayerStack.PopLayer(layer);
		layer->OnDetach();
	}

	void Application::PopOverlay(Layer* layer)
	{
		m_LayerStack.PopOverlay(layer);
		layer->OnDetach();
	}

	void Application::RenderImGui()
	{
		X2_PROFILE_FUNC();
		X2_SCOPE_PERF("Application::RenderImGui");

		m_ImGuiLayer->Begin();

		for (int i = 0; i < m_LayerStack.Size(); i++)
			m_LayerStack[i]->OnImGuiRender();
	}

	void Application::Run()
	{
		OnInit();
		while (m_Running)
		{
			X2_PROFILE_FRAME("MainThread");

			// Wait for render thread to finish frame
			{
				X2_PROFILE_FUNC("Wait");
				Timer timer;

				m_RenderThread.BlockUntilRenderComplete();

				m_PerformanceTimers.MainThreadWaitTime = timer.ElapsedMillis();
			}

			static uint64_t frameCounter = 0;
			//X2_CORE_INFO("-- BEGIN FRAME {0}", frameCounter);

			ProcessEvents(); // Poll events when both threads are idle

			m_ProfilerPreviousFrameData = m_Profiler->GetPerFrameData();
			m_Profiler->Clear();

			m_RenderThread.NextFrame();

			// Start rendering previous frame
			m_RenderThread.Kick();

			if (!m_Minimized)
			{
				Timer cpuTimer;

				// On Render thread
				Renderer::Submit([&]()
					{
						m_Window->GetSwapChain().BeginFrame();
					});

				Renderer::BeginFrame();
				{
					X2_SCOPE_PERF("Application Layer::OnUpdate");
					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(m_TimeStep);
				}

				// Render ImGui on render thread
				Application* app = this;
				if (m_Specification.EnableImGui)
				{
					Renderer::Submit([app]() { app->RenderImGui(); });
					Renderer::Submit([=]() { m_ImGuiLayer->End(); });
				}
				Renderer::EndFrame();

				// On Render thread
				Renderer::Submit([&]()
					{
						// m_Window->GetSwapChain().BeginFrame();
						// Renderer::WaitAndRender();
						m_Window->SwapBuffers();
					});

				m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % Renderer::GetConfig().FramesInFlight;
				m_PerformanceTimers.MainThreadWorkTime = cpuTimer.ElapsedMillis();
			}

			//ScriptEngine::InitializeRuntimeDuplicatedEntities();
			Input::ClearReleasedKeys();

			float time = GetTime();
			m_Frametime = time - m_LastFrameTime;
			m_TimeStep = glm::min<float>(m_Frametime, 0.0333f);
			m_LastFrameTime = time;

			//X2_CORE_INFO("-- END FRAME {0}", frameCounter);
			frameCounter++;
		}
		OnShutdown();
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::OnShutdown()
	{
		m_EventCallbacks.clear();
		g_ApplicationRunning = false;
	}

	void Application::ProcessEvents()
	{
		Input::TransitionPressedKeys();
		Input::TransitionPressedButtons();

		m_Window->ProcessEvents();

		std::scoped_lock<std::mutex> lock(m_EventQueueMutex);

		// Process custom event queue
		while (m_EventQueue.size() > 0)
		{
			auto& func = m_EventQueue.front();
			func();
			m_EventQueue.pop();
		}
	}

	void Application::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return OnWindowResize(e); });
		dispatcher.Dispatch<WindowMinimizeEvent>([this](WindowMinimizeEvent& e) { return OnWindowMinimize(e); });
		dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { return OnWindowClose(e); });

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(event);
			if (event.Handled)
				break;
		}

		if (event.Handled)
			return;

		// TODO(Peter): Should these callbacks be called BEFORE the layers recieve events?
		//				We may actually want that since most of these callbacks will be functions REQUIRED in order for the game
		//				to work, and if a layer has already handled the event we may end up with problems
		for (auto& eventCallback : m_EventCallbacks)
		{
			eventCallback(event);

			if (event.Handled)
				break;
		}

	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		const uint32_t width = e.GetWidth(), height = e.GetHeight();
		if (width == 0 || height == 0)
		{
			//m_Minimized = true;
			return false;
		}
		//m_Minimized = false;

		auto& window = m_Window;
		Renderer::Submit([&window, width, height]() mutable
			{
				window->GetSwapChain().OnResize(width, height);
			});

		return false;
	}

	bool Application::OnWindowMinimize(WindowMinimizeEvent& e)
	{
		m_Minimized = e.IsMinimized();
		return false;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		Close();
		return false; // give other things a chance to react to window close
	}

	float Application::GetTime() const
	{
		return (float)glfwGetTime();
	}

	const char* Application::GetConfigurationName()
	{
#if defined(X2_DEBUG)
		return "Debug";
#elif defined(X2_RELEASE)
		return "Release";
#elif defined(X2_DIST)
		return "Dist";
#else
#error Undefined configuration?
#endif
	}

	const char* Application::GetPlatformName()
	{
#if defined(X2_PLATFORM_WINDOWS)
		return "Windows x64";
#else
#error Undefined platform?
#endif
	}

}
