#pragma once

#include "Toast/Core/Base.h"

#include "Toast/Core/Window.h"
#include "Toast/Core/LayerStack.h"
#include "Toast/Core/Timestep.h"

#include "Toast/Events/Event.h"
#include "Toast/Events/ApplicationEvent.h"

#include "Toast/ImGui/ImGuiLayer.h"

#define NOMINMAX
#include <windows.h>

int main(int argc, char** argv);

namespace Toast {

	struct ApplicationSpecification
	{
		std::string Name = "Toast Application";
		std::string WorkingDirectory;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecification& specification);
		virtual ~Application();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		Window& GetWindow() { return *mWindow; }

		void Close();

		ImGuiLayer* GetImGuiLayer() { return mImGuiLayer; }

		static Application& Get() { return *sInstance; }

		static const char* GetConfigurationName();
		static const char* GetPlatformName();

		const ApplicationSpecification& GetSpecification() { return mSpecification; }
	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		ApplicationSpecification mSpecification;
		std::unique_ptr<Window> mWindow;
		ImGuiLayer* mImGuiLayer;
		bool mRunning = true;
		bool mMinimized = false;
		LayerStack mLayerStack;
		float mLastFrameTime = 0.0f;

		LARGE_INTEGER mStartTime;
	private:
		static Application* sInstance;
		friend int ::main(int argc, char** argv);
	};

	// To be defined in client
	Application* CreateApplication();
}