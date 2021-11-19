#include "SceneSettingsPanel.h"

#include "../FontAwesome.h"

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
		ImGui::Begin(ICON_TOASTER_COG" Settings");

		if(mContext)
		{
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

			ImGui::Text("Selection mode");
			ImGui::SameLine();

			char* label = mSelectionMode == SelectionMode::Entity ? "Entity" : "Mesh";
			if (ImGui::Button(label))
			{
				mSelectionMode = mSelectionMode == SelectionMode::Entity ? SelectionMode::SubMesh : SelectionMode::Entity;
			}

			ImGui::Checkbox("Show grid", &mContext->mSettings.Grid);
			ImGui::Checkbox("Show camera frustum", &mContext->mSettings.CameraFrustum);
		}

		ImGui::End();
	}

}