#pragma once

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
		SceneSettingsPanel(const Ref<Scene>& context);
		~SceneSettingsPanel() = default;

		void SetContext(const Ref<Scene>& context);

		void OnImGuiRender();

		SelectionMode GetSelectionMode() { return mSelectionMode; }
	private:
		Ref<Scene> mContext;
		
		SelectionMode mSelectionMode = SelectionMode::Entity;
	};

}