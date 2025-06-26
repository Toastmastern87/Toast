#pragma once

#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Renderer/PlanetSystem.h"
#include "Toast/Scene/Scene.h"

namespace Toast {

	class PlanetPanel
	{
	public:
		PlanetPanel() = default;
		~PlanetPanel() = default;

		void OnImGuiRender(bool* showPanel, std::string& activeDragArea);

		void SetContext(Scene* context, WindowsWindow* window);
	private:
		Scene* mContext;

		WindowsWindow* mWindow;
	};

}