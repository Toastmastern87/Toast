#pragma once

#include "Core.h"
#include "Events/Event.h"
#include "Toast/Events/ApplicationEvent.h"

#include "Window.h"

namespace Toast {
	class TOAST_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> mWindow;
		bool mRunning = true;
	};

	// To be defined in client
	Application* CreateApplication();
}