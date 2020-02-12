#pragma once

#ifdef TOAST_PLATFORM_WINDOWS

extern Toast::Application* Toast::CreateApplication();

int main(int argv, char** argc)
{
	auto app = Toast::CreateApplication();
	app->Run();
	delete app;
}

#endif 
