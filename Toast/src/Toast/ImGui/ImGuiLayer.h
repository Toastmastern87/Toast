#pragma once

#include "Toast/Layer.h"

#include "Toast/Events/ApplicationEvent.h"
#include "Toast/Events/KeyEvent.h"
#include "Toast/Events/MouseEvent.h"

namespace Toast 
{
	class TOAST_API ImGuiLayer : public Layer 
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		void OnAttach();
		void OnDetach();
		void OnUpdate();
		void OnEvent(Event& e);
	private:
		bool OnMouseButtonPressedEvent(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e);
		bool OnMouseMovedEvent(MouseMovedEvent& e);
		bool OnMouseScrolledEvent(MouseScrolledEvent& e);
		bool OnKeyPressedEvent(KeyPressedEvent& e);
		bool OnKeyReleasedEvent(KeyReleasedEvent& e);
		bool OnKeyTypedEvent(KeyTypedEvent& e);
		bool OnWindowResizeEvent(WindowResizeEvent& e);
	private:
		DWORD mOldTime, mCurrentTime;
	};
}