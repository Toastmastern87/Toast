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

		inline Window& GetWindow() { return *mWindow; }

		inline static Application& Get() { return *sInstance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> mWindow;
		bool mRunning = true;
		LayerStack mLayerStack;

	private:
		static Application* sInstance;
	};

	// To be defined in client
	Application* CreateApplication();
}