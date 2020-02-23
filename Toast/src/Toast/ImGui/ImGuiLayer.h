#pragma once

#include "Toast/Layer.h"

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
		DWORD mOldTime, mCurrentTime;
	};
}