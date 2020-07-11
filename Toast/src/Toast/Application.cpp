#include "tpch.h"
#include "Application.h"

#include "Toast/Log.h"

#include "Toast/Input.h"

namespace Toast {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::sInstance = nullptr;
	
	Application::Application()
	{
		TOAST_CORE_ASSERT(!sInstance, "Application already exists");

		sInstance = this;

		mWindow = std::unique_ptr<Window>(Window::Create());
		mWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));

		mImGuiLayer = new ImGuiLayer();
		PushOverlay(mImGuiLayer);

		mShader.reset(new Shader("../Toast/src/Toast/Renderer/ShaderTest_Vs.hlsl", "../Toast/src/Toast/Renderer/ShaderTest_ps.hlsl"));
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
			mWindow->Start();

			mShader->Bind();

			for (Layer* layer : mLayerStack) 
				layer->OnUpdate();

			mImGuiLayer->Begin();
			for (Layer* layer : mLayerStack) 
				layer->OnImGuiRender();
			mImGuiLayer->End();

			mWindow->End();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		mRunning = false;

		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		mWindow->OnResize((UINT)(e.GetWidth()), (UINT)(e.GetHeight()));

		return true;
	}
}