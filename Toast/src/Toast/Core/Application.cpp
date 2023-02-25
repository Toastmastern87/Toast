#include "tpch.h"
#include "Toast/Core/Application.h"

#include "Toast/Core/Input.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/UI/Font.h"

#include "Toast/Scripting/ScriptEngine.h"

namespace Toast {

	Application* Application::sInstance = nullptr;

	Application::Application(const ApplicationSpecification& specification)
		: mSpecification(specification)
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_ASSERT(!sInstance, "Application already exists");

		QueryPerformanceCounter(&mStartTime);

		sInstance = this;

		if (!mSpecification.WorkingDirectory.empty())
			std::filesystem::current_path(mSpecification.WorkingDirectory);

		// Get the resolution of the screen
		RECT desktop;
		const HWND hDesktop = GetDesktopWindow();
		GetWindowRect(hDesktop, &desktop);

		mWindow = Window::Create(WindowProps(mSpecification.Name, desktop.right, desktop.bottom));
		mWindow->SetEventCallback(TOAST_BIND_EVENT_FN(Application::OnEvent));
		mWindow->SetVSync(false);

		Renderer::Init();

		ScriptEngine::Init();
		Font::StaticInit();

		mImGuiLayer = new ImGuiLayer();
		PushOverlay(mImGuiLayer);
	}
	
	Application::~Application() 
	{
		TOAST_PROFILE_FUNCTION();

		ScriptEngine::Shutdown();

		Renderer::Shutdown();

		RenderCommand::CleanUp();
	}

	void Application::PushLayer(Layer* layer) 
	{
		TOAST_PROFILE_FUNCTION();

		mLayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		TOAST_PROFILE_FUNCTION();

		mLayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::Close()
	{
		mRunning = false;
	}

	const char* Application::GetConfigurationName()
	{
#if defined(TOAST_DEBUG)
		return "Debug";
#elif defined(TOAST_RELEASE)
		return "Release";
#elif defined(TOAST_DIST)
		return "Dist";
#else
	#error Undefined configuration
#endif
	}

	const char* Application::GetPlatformName()
	{
#if defined(TOAST_PLATFORM_WINDOWS)
		return "Windows x64";
#else
	#error Undefined platform
#endif
	}

	void Application::SubmitToMainThread(const std::function<void()>& function)
	{
		std::scoped_lock<std::mutex> lock(mMainThreadQueueMutex);

		mMainThreadQueue.emplace_back(function);
	}

	void Application::OnEvent(Event& e)
	{
		TOAST_PROFILE_FUNCTION();

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(TOAST_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(TOAST_BIND_EVENT_FN(Application::OnWindowResize));

		for (auto it = mLayerStack.rbegin(); it != mLayerStack.rend(); ++it) 
		{
			if (e.Handled)
				break;
			(*it)->OnEvent(e);
		}
	}

	void Application::Run() 
	{
		TOAST_PROFILE_FUNCTION();

		while (mRunning) 
		{
			TOAST_PROFILE_SCOPE("RunLoop");

			LARGE_INTEGER currentTime, frequency;
			QueryPerformanceCounter(&currentTime);
			QueryPerformanceFrequency(&frequency);
			
			float time = static_cast<float>((currentTime.QuadPart - mStartTime.QuadPart) / (double)frequency.QuadPart);
			Timestep timestep = time - mLastFrameTime;
			mLastFrameTime = time;

			ExecuteMainThreadQueue();

			if (!mMinimized) 
			{
				{
					TOAST_PROFILE_SCOPE("LayerStack OnUpdate");

					for (Layer* layer : mLayerStack)
						layer->OnUpdate(timestep);
				}

				mImGuiLayer->Begin();
				{
					TOAST_PROFILE_SCOPE("LayerStack OnImGuiRender");

					for (Layer* layer : mLayerStack)
						layer->OnImGuiRender();
				}
				mImGuiLayer->End();
			}

			mWindow->OnUpdate();
		}

		mImGuiLayer->OnDetach();
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		mRunning = false;

		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		TOAST_PROFILE_FUNCTION();

		if (e.GetWidth() == 0 || e.GetHeight() == 0) 
		{
			mMinimized = true;
			return false;
		}

		mMinimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());

		return false;
	}

	void Application::ExecuteMainThreadQueue()
	{
		std::scoped_lock<std::mutex> lock(mMainThreadQueueMutex);

		for (auto& func : mMainThreadQueue)
			func();

		mMainThreadQueue.clear();
	}

}