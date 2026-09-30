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
	
	auto app = Toast::CreateApplication();

	app->Run();
	TOAST_PROFILE_END_SESSION();

	Toast::Log::Shutdown();

	delete app;
}

#endif 
