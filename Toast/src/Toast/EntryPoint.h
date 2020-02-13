#pragma once

#ifdef TOAST_PLATFORM_WINDOWS

extern Toast::Application* Toast::CreateApplication();

int main(int argv, char** argc)
{
	Toast::Log::Init();
	TOAST_CORE_WARN("Initialized Log!");
	int a = 1337;
	TOAST_INFO("Hello! Var={0}", a);

	auto app = Toast::CreateApplication();
	app->Run();
	delete app;
}

#endif 
