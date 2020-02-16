#include "tpch.h"
#include "Application.h"

#include "Toast/Events/ApplicationEvent.h"
#include "Toast/Log.h"

#include <GLFW/glfw3.h>

namespace Toast {
	Application::Application()
	{
		mWindow = std::unique_ptr<Window>(Window::Create());
	}

	Application::~Application()
	{
	}

	void Application::Run() 
	{
		while (mRunning) 
		{
			glClearColor(0, 1, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			mWindow->OnUpdate();
		}
	}
}