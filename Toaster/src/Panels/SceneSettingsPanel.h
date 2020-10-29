#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Scene/Scene.h"


namespace Toast {

	class SceneSettingsPanel
	{
	public:
		SceneSettingsPanel() = default;
		SceneSettingsPanel(const Ref<Scene>& context);
		~SceneSettingsPanel() = default;

		void SetContext(const Ref<Scene>& context);

		void OnImGuiRender();
	private:
		Ref<Scene> mContext;
	};

}