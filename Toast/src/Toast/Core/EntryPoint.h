#pragma once
#include "Toast/Core/Base.h"
#include <crtdbg.h>

#ifdef TOAST_PLATFORM_WINDOWS

extern Toast::Application* Toast::CreateApplication();

int main(int argv, char** argc)
{
	// TURN ON TO ACTIVATE HEAP DEBUGGING
	//int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

	//// turn on debug‐heap allocations and leak‐checking
	//flags |= _CRTDBG_ALLOC_MEM_DF
	//	| _CRTDBG_LEAK_CHECK_DF

	//	// check the heap *every Nth* allocation instead of always
	//	| _CRTDBG_CHECK_EVERY_16_DF;

	//_CrtSetDbgFlag(flags);

	Toast::Log::Init();
	
	TOAST_PROFILE_BEGIN_SESSION("Startup", "ToastProfile-Startup.json");
	auto app = Toast::CreateApplication();
	TOAST_PROFILE_END_SESSION();

	TOAST_PROFILE_BEGIN_SESSION("Runtime", "ToastProfile-Runtime.json");
	app->Run();
	TOAST_PROFILE_END_SESSION();

	Toast::Log::Shutdown();

	TOAST_PROFILE_BEGIN_SESSION("Shutdown", "ToastProfile-Shutdown.json");
	delete app;
	TOAST_PROFILE_END_SESSION();
}

#endif 
