#include "tpch.h"
#include "Toast/Core/Application.h"

#include "Toast/Core/Log.h"

#include "Toast/Core/Input.h"

#include "Toast/Renderer/Renderer.h"

namespace Toast {

	Application* Application::sInstance = nullptr;

	Application::Application(const std::string& name)
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_ASSERT(!sInstance, "Application already exists");

		QueryPerformanceCounter(&mStartTime);

		sInstance = this;

		mWindow = Window::Create(WindowProps(name));
		mWindow->SetEventCallback(TOAST_BIND_EVENT_FN(Application::OnEvent));

		Renderer::Init();

		mImGuiLayer = new ImGuiLayer();
		PushOverlay(mImGuiLayer);
	}
	
	Application::~Application() 
	{
		TOAST_PROFILE_FUNCTION();

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
}