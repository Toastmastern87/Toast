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
		//ImGui::ShowDemoWindow();

		ImGui::Begin("Scene Settings");

		if(mContext)
		{
			ImGui::Checkbox("Grid Activated ", &mContext->mSettings.GridActivated);

			ImGui::Separator();

			if (ImGui::RadioButton("Normal", mContext->mSettings.WireframeRendering == Scene::Settings::Wireframe::NO))
			{
				mContext->mSettings.WireframeRendering = Scene::Settings::Wireframe::NO;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Wireframe", mContext->mSettings.WireframeRendering == Scene::Settings::Wireframe::YES)) 
			{
				mContext->mSettings.WireframeRendering = Scene::Settings::Wireframe::YES;
			}  
			ImGui::SameLine();
			if (ImGui::RadioButton("Wireframe Overlay", mContext->mSettings.WireframeRendering == Scene::Settings::Wireframe::ONTOP))
			{
				mContext->mSettings.WireframeRendering = Scene::Settings::Wireframe::ONTOP; 
			}
		}

		ImGui::End();
	}

}