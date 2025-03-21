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
		std::string IconStr = "Resources/ToasterIcon32x32.png";
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

		void SubmitToMainThread(const std::function<void()>& function);
	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

		void ExecuteMainThreadQueue();
	private:
		ApplicationSpecification mSpecification;
		std::unique_ptr<Window> mWindow;
		ImGuiLayer* mImGuiLayer;
		bool mRunning = true;
		bool mMinimized = false;
		LayerStack mLayerStack;
		float mLastFrameTime = 0.0f;

		LARGE_INTEGER mStartTime;

		std::vector<std::function<void()>> mMainThreadQueue;
		std::mutex mMainThreadQueueMutex;
	private:
		static Application* sInstance;
		friend int ::main(int argc, char** argv);
	};

	// To be defined in client
	Application* CreateApplication();
}