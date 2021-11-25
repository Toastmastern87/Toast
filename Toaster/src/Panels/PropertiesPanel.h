#pragma once

#include "Toast/Core/Base.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Scene.h"

#include "SceneHierarchyPanel.h"

namespace Toast {

	class PropertiesPanel
	{
	public:
		PropertiesPanel() = default;
		PropertiesPanel(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel);
		~PropertiesPanel() = default;

		void SetContext(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel);

		void OnImGuiRender();
	private:
		void DrawComponents(Entity entity);
	private:
		Entity mContext;
		Scene* mScene;

		SceneHierarchyPanel* mSceneHierarchyPanel;
	};

}