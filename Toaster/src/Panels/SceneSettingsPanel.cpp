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
			const char* items[] = { "None", "Albedo", "Height Map", "Normal" };
			int currentOverlay = static_cast<int>(mContext->mSettings.PlanetOverlaySetting);

			ImGui::Text("Render Overlay");
			ImGui::SameLine();
			if (ImGui::Combo("", &currentOverlay, items, IM_ARRAYSIZE(items)))
				mContext->mSettings.PlanetOverlaySetting = static_cast<Scene::Settings::PlanetOverlay>(currentOverlay);

			auto& wireframeButton = [&](const char* label, Scene::Settings::Wireframe mode)
			{
				if (ImGui::RadioButton(label, mContext->mSettings.WireframeRendering == mode))
					mContext->mSettings.WireframeRendering = mode;
			};

			wireframeButton("Normal", Scene::Settings::Wireframe::NO);
			ImGui::SameLine();
			wireframeButton("Wireframe", Scene::Settings::Wireframe::YES);

			ImGui::Text("Selection mode");
			ImGui::SameLine();

			char* label = mSelectionMode == SelectionMode::Entity ? "Entity" : "Mesh";
			if (ImGui::Button(label))
			{
				mSelectionMode = mSelectionMode == SelectionMode::Entity ? SelectionMode::SubMesh : SelectionMode::Entity;
			}

			ImGui::Checkbox("Show grid", &mContext->mSettings.Grid);
			ImGui::Checkbox("Show camera frustum", &mContext->mSettings.CameraFrustum);
			if(ImGui::Checkbox("Planet backface culling", &mContext->mSettings.BackfaceCulling))
				mContext->mSettings.IsDirty = true;
			if(ImGui::Checkbox("Planet frustum culling", &mContext->mSettings.FrustumCulling))
				mContext->mSettings.IsDirty = true;
			ImGui::Checkbox("Render Colliders", &mContext->mSettings.RenderColliders);
			ImGui::Checkbox("Render UI", &mContext->mSettings.RenderUI);
		}

		ImGui::End();
	}

}