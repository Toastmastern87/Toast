#pragma once

#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Core/Base.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Scene.h"

#include "SceneHierarchyPanel.h"

namespace Toast {

	class PropertiesPanel
	{
	public:
		PropertiesPanel() = default;
		PropertiesPanel(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel, WindowsWindow* window);
		~PropertiesPanel() = default;

		void SetContext(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel, WindowsWindow* window);

		void OnImGuiRender(std::string& activeDragArea);
	private:
		void DrawComponents(Entity entity, std::string& activeDragArea);
	private:
		Entity mContext;
		Scene* mScene;

		WindowsWindow* mWindow;

		SceneHierarchyPanel* mSceneHierarchyPanel;
	};

}