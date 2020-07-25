#include "tpch.h"
#include "Application.h"

#include "Toast/Log.h"

#include "Toast/Input.h"

#include "Toast/Renderer/Renderer.h"

namespace Toast {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::sInstance = nullptr;

	Application::Application()
	{
		TOAST_CORE_ASSERT(!sInstance, "Application already exists");

		QueryPerformanceCounter(&mStartTime);

		sInstance = this;

		mWindow = std::unique_ptr<Window>(Window::Create());
		mWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));

		mImGuiLayer = new ImGuiLayer();
		PushOverlay(mImGuiLayer);
	}

	void Application::PushLayer(Layer* layer) 
	{
		mLayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* layer)
	{
		mLayerStack.PushOverlay(layer);
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));

		for (auto it = mLayerStack.end(); it != mLayerStack.begin(); ) 
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run() 
	{
		while (mRunning) 
		{
			LARGE_INTEGER currentTime, frequency;
			QueryPerformanceCounter(&currentTime);
			QueryPerformanceFrequency(&frequency);
			
			float time = static_cast<float>((currentTime.QuadPart - mStartTime.QuadPart) / (double)frequency.QuadPart);
			Timestep timestep = time - mLastFrameTime;
			mLastFrameTime = time;

			for (Layer* layer : mLayerStack) 
				layer->OnUpdate(timestep);

			mImGuiLayer->Begin();
			for (Layer* layer : mLayerStack) 
				layer->OnImGuiRender();
			mImGuiLayer->End();

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
		mWindow->Resize((UINT)(e.GetWidth()), (UINT)(e.GetHeight()));

		return true;
	}
}