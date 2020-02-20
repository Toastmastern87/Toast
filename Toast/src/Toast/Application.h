#pragma once

#include "Window.h"
#include "Toast/LayerStack.h"
#include "Core.h"
#include "Events/Event.h"
#include "Toast/Events/ApplicationEvent.h"

namespace Toast {
	class TOAST_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> mWindow;
		bool mRunning = true;
		LayerStack mLayerStack;
	};

	// To be defined in client
	Application* CreateApplication();
}