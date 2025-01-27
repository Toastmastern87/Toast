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
		SceneSettingsPanel(const Ref<Scene>& context, WindowsWindow* window);
		~SceneSettingsPanel() = default;

		void SetContext(const Ref<Scene>& context, WindowsWindow* window);

		void OnImGuiRender(std::string& activeDragArea);

		SelectionMode GetSelectionMode() { return mSelectionMode; }
	private:
		Ref<Scene> mContext;

		WindowsWindow* mWindow;
		
		SelectionMode mSelectionMode = SelectionMode::Entity;
	};

}