#pragma once
#include "Toast/Core/Core.h"

#ifdef TOAST_PLATFORM_WINDOWS

extern Toast::Application* Toast::CreateApplication();

int main(int argv, char** argc)
{
	Toast::Log::Init();
	TOAST_CORE_WARN("Initialized Log!");

	auto app = Toast::CreateApplication();
	app->Run();
	delete app;
}

#endif 
