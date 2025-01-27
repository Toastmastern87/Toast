#pragma once

#include "Toast/Core/Layer.h"

#include "Toast/Events/ApplicationEvent.h"
#include "Toast/Events/KeyEvent.h"
#include "Toast/Events/MouseEvent.h"

namespace Toast 
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer(WindowsWindow* window = nullptr);
		~ImGuiLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		void BlockEvents(bool block) { mBlockEvents = block; }

		void SetDarkThemeColors();
	private:
		bool mBlockEvents = true;
	};
}