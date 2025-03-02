#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/Log.h"
#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"

#include <DirectXMath.h>

#include <imgui\imgui.h>

namespace Toast {

	class SceneHierarchyPanel 
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene>& context);
		Scene* GetContext() const { return mContext.get(); }

		void OnImGuiRender();

		void SetSelectedEntity(Entity entity);
		Entity GetSelectedEntity() const { return mSelectionContext; }
	private:
		void DrawEntityNode(Entity entity);
	private:
		Ref<Scene> mContext;
		Entity mSelectionContext;

		Entity mEntityBeingRenamed;
		char mRenameBuffer[256];
		bool mSetRenameFocus = false;

		Entity mPrefabEntity;
		bool mShowPrefabPopup = false;
		ImVec2 mPrefabPopupPos;
	};
}