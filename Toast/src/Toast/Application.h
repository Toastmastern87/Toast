#pragma once

#include "Core.h"
#include "Events/Event.h"
#include "Window.h"

namespace Toast {
	class TOAST_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	private:
		std::unique_ptr<Window> mWindow;
		bool mRunning = true;
	};

	// To be defined in client
	Application* CreateApplication();
}