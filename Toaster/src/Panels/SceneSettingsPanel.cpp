#include "SceneSettingsPanel.h"

#include "imgui/imgui.h"

namespace Toast {

	SceneSettingsPanel::SceneSettingsPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneSettingsPanel::SetContext(const Ref<Scene>& context)
	{
		mContext = context;
	}

	void SceneSettingsPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Settings");

		if(mContext)
		{
			ImGui::Checkbox("Grid Activated ", &mContext->mSettings.GridActivated);

			ImGui::Separator();

			auto& wireframeButton = [&](const char* label, Scene::Settings::Wireframe mode)
			{
				if (ImGui::RadioButton(label, mContext->mSettings.WireframeRendering == mode))
					mContext->mSettings.WireframeRendering = mode;
			};

			wireframeButton("Normal", Scene::Settings::Wireframe::NO);
			ImGui::SameLine();
			wireframeButton("Wireframe", Scene::Settings::Wireframe::YES);
			ImGui::SameLine();
			wireframeButton("Wireframe Overlay", Scene::Settings::Wireframe::ONTOP);
		}

		ImGui::End();
	}

}