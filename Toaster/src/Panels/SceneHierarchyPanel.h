#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/Log.h"
#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"

#include <DirectXMath.h>

namespace Toast {

	class SceneHierarchyPanel 
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene>& context);

		void OnImGuiRender();

		void SetSelectedEntity(Entity entity);
		Entity GetSelectedEntity() const { return mSelectionContext; }
	private:
		void DrawEntityNode(Entity entity);
		void DrawComponents(Entity entity);
	private:
		Ref<Scene> mContext;
		Entity mSelectionContext;
	};
}