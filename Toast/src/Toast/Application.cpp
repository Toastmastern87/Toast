#include "tpch.h"
#include "Application.h"

#include "Toast/Events/ApplicationEvent.h"
#include "Toast/Log.h"

namespace Toast {
	Application::Application()
	{
	}

	Application::~Application()
	{
	}

	void Application::Run() 
	{
		WindowResizeEvent e(1280, 720); 
		TOAST_TRACE(e);

		while (true);
	}
}