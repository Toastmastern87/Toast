#pragma once
#include "Toast/Core/Core.h"

#ifdef TOAST_PLATFORM_WINDOWS

extern Toast::Application* Toast::CreateApplication();

int main(int argv, char** argc)
{
	Toast::Log::Init();
	
	TOAST_PROFILE_BEGIN_SESSION("Startup", "ToastProfile-Startup.json");
	auto app = Toast::CreateApplication();
	TOAST_PROFILE_END_SESSION();

	TOAST_PROFILE_BEGIN_SESSION("Runtime", "ToastProfile-Runtime.json");
	app->Run();
	TOAST_PROFILE_END_SESSION();

	TOAST_PROFILE_BEGIN_SESSION("Shutdown", "ToastProfile-Shutdown.json");
	delete app;
	TOAST_PROFILE_END_SESSION();
}

#endif 
