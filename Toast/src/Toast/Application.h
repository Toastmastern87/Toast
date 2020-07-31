#pragma once

#include "Core.h"

#include "Window.h"
#include "Toast/LayerStack.h"
#include "Toast/Events/Event.h"
#include "Toast/Events/ApplicationEvent.h"

#include "Toast/Core/Timestep.h"

#include "Toast/ImGui/ImGuiLayer.h"

namespace Toast {
	class TOAST_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline Window& GetWindow() { return *mWindow; }

		inline static Application& Get() { return *sInstance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		std::unique_ptr<Window> mWindow;
		ImGuiLayer* mImGuiLayer;
		bool mRunning = true;
		LayerStack mLayerStack;
		float mLastFrameTime;

		LARGE_INTEGER mStartTime;
	private:
		static Application* sInstance;
	};

	// To be defined in client
	Application* CreateApplication();
}