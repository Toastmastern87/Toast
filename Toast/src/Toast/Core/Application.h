#pragma once

#include "Toast/Core/Base.h"

#include "Toast/Core/Window.h"
#include "Toast/Core/LayerStack.h"
#include "Toast/Events/Event.h"
#include "Toast/Events/ApplicationEvent.h"

#include "Toast/Core/Timestep.h"

#include "Toast/ImGui/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Toast {
	class Application
	{
	public:
		Application(const std::string& name = "Toast App");
		virtual ~Application();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		std::string OpenFile(const char* filter = "All\0*.*\0") const;
		std::string SaveFile(const char* filter = "All\0*.*\0") const;

		Window& GetWindow() { return *mWindow; }

		void Close();

		ImGuiLayer* GetImGuiLayer() { return mImGuiLayer; }

		static Application& Get() { return *sInstance; }

		static const char* GetConfigurationName();
		static const char* GetPlatformName();
	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
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