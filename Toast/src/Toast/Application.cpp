#include "tpch.h"
#include "Application.h"

#include "Toast/Log.h"

namespace Toast {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::sInstance = nullptr;
	
	Application::Application()
	{
		TOAST_CORE_ASSERT(!sInstance, "Application already exists");

		sInstance = this;

		mWindow = std::unique_ptr<Window>(Window::Create());
		mWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));
	}

	Application::~Application()
	{
	}

	void Application::PushLayer(Layer* layer) 
	{
		mLayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		mLayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));

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
			for (Layer* layer : mLayerStack)
				layer->OnUpdate();

			mWindow->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		mRunning = false;

		return true;
	}
}