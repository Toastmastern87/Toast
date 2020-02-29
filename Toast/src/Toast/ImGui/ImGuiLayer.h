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

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;

		void Begin();
		void End();
	};
}