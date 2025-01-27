#include "SceneSettingsPanel.h"

#include "../FontAwesome.h"

#include "Toast/ImGui/ImGuiHelpers.h"

#include "imgui/imgui.h"

namespace Toast {

	SceneSettingsPanel::SceneSettingsPanel(const Ref<Scene>& context, WindowsWindow* window)
	{
		SetContext(context, window);
	}

	void SceneSettingsPanel::SetContext(const Ref<Scene>& context, WindowsWindow* window)
	{
		mContext = context;

		mWindow = window;
	}

	void SceneSettingsPanel::OnImGuiRender(std::string& activeDragArea)
	{
		ImGui::Begin(ICON_TOASTER_COG" Settings");

		if(mContext)
		{
			const char* items[] = { "None", "Albedo", "Height Map", "Normal", "DebugColor" };
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
			ImGui::Checkbox("Show sun light frustum", &mContext->mSettings.SunLightFrustum);
			ImGui::Checkbox("Shadows", &mContext->mSettings.Shadows);
			ImGui::Checkbox("SSAO", &mContext->mSettings.SSAO);
			ImGui::Checkbox("Dynamic IBL", &mContext->mSettings.DynamicIBL);
			if(ImGui::Checkbox("Planet backface culling", &mContext->mSettings.BackfaceCulling))
				mContext->mSettings.IsDirty = true;
			if(ImGui::Checkbox("Planet frustum culling", &mContext->mSettings.FrustumCulling))
				mContext->mSettings.IsDirty = true;
			ImGui::Checkbox("Render Colliders", &mContext->mSettings.RenderColliders);
			ImGui::Checkbox("Render UI", &mContext->mSettings.RenderUI);

			ImGui::Text("Physics slow motion");
			ImGui::SliderInt("##physicsslowmotion", &mContext->mSettings.PhysicSlowmotion, 1, 30);

			ImGui::Text("Sun Frustum Ortho Size");
			ImGuiHelpers::ManualDragFloat("##sunlightdistance", mContext->mSettings.SunFrustumOrthoSize, mWindow, activeDragArea, 10.0f, ImVec2{ 255.0f, 20.0f }, "%.1f", 50.0f, 10000.0f);
		}

		ImGui::End();
	}

}