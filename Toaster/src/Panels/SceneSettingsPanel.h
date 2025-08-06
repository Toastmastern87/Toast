#pragma once

#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Core/Base.h"
#include "Toast/Scene/Scene.h"

namespace Toast {

	class SceneSettingsPanel
	{
	public:
		enum class SelectionMode
		{
			None = 0, Entity = 1, SubMesh = 2
		};

		SceneSettingsPanel() = default;
		SceneSettingsPanel(Scene* context, WindowsWindow* window);
		~SceneSettingsPanel() = default;

		void SetContext(Scene* context, WindowsWindow* window);

		void OnImGuiRender(bool* showPanel, std::string& activeDragArea);

		SelectionMode GetSelectionMode() { return mSelectionMode; }
	private:
		Scene* mContext;

		WindowsWindow* mWindow;
		
		SelectionMode mSelectionMode = SelectionMode::Entity;
	};

}