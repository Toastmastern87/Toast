#include "SceneSettingsPanel.h"

#include "../FontAwesome.h"

#include "Toast/ImGui/ImGuiHelpers.h"

#include "imgui/imgui.h"

namespace Toast {

	SceneSettingsPanel::SceneSettingsPanel(Scene* context, WindowsWindow* window)
	{
		SetContext(context, window);
	}

	void SceneSettingsPanel::SetContext(Scene* context, WindowsWindow* window)
	{
		mContext = context;

		mWindow = window;
	}

	void SceneSettingsPanel::OnImGuiRender(bool* showPanel, std::string& activeDragArea)
	{
		if (!showPanel || !*showPanel)
			return;

		ImGuiIO& io = ImGui::GetIO();

		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		const ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar;

		if (ImGui::Begin("SceneSettings", nullptr, popupFlags))
		{
			const float titleBarHeight = 38.0f;
			const float buttonSize = 24.0f;

			ImVec2 windowPos = ImGui::GetWindowPos();
			ImVec2 windowSize = ImGui::GetWindowSize();

			ImVec2 titleBarMin = windowPos;
			ImVec2 titleBarMax = ImVec2(windowPos.x + windowSize.x, windowPos.y + titleBarHeight);
			ImU32 titleBarColor = ImGui::GetColorU32(ImGuiCol_Header);
			ImGui::GetWindowDrawList()->AddRectFilled(titleBarMin, titleBarMax, titleBarColor);

			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 12.0f, windowPos.y + 8.0f));
			ImGui::PushFont(io.Fonts->Fonts[3]);
			ImGui::Text(ICON_TOASTER_COG" Scene Settings");
			ImGui::PopFont();

			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + windowSize.x - buttonSize - 6.0f, windowPos.y + 6.0f));
			if (ImGui::Button("X##SceneSettingsClose", ImVec2(buttonSize, buttonSize)))
				*showPanel = false;

			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 10.0f, windowPos.y + titleBarHeight + 6.0f));

			ImGui::Spacing();
			ImGui::Indent(10.0f);

			if (mContext)
			{
				const char* items[] = { "None", "G-Buffer Positions", "G-Buffer Normals", "G-Buffer Albedo/Metallic", "Roughness", "Lighting Pass Output", "Atmospheric Scattering Output", "SSAO", "SSAO Blur", "Bloom", "Bloom Blur", "Bloom Final" };
				int currentOverlay = static_cast<int>(mContext->mSettings.RenderOverlaySetting);

				ImGui::Text("Render Overlay");
				ImGui::SameLine();
				if (ImGui::Combo("", &currentOverlay, items, IM_ARRAYSIZE(items)))
					mContext->mSettings.RenderOverlaySetting = static_cast<RenderOverlay>(currentOverlay);

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
				ImGui::Checkbox("SSAODebugging", &mContext->mSettings.SSAODebugging);
				ImGui::Text("SSAO Radius");
				ImGuiHelpers::ManualDragFloat("##ssaoradius", mContext->mSettings.SSAORadius, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 50.0f);
				ImGui::Text("SSAO bias");
				ImGuiHelpers::ManualDragFloat("##ssaobias", mContext->mSettings.SSAObias, mWindow, activeDragArea, 0.001f, ImVec2{ 255.0f, 20.0f }, "%.4f", -1.0f, 1.0f);
				ImGui::Checkbox("Bloom", &mContext->mSettings.Bloom);
				ImGui::Text("Bloom Intensity");
				ImGuiHelpers::ManualDragFloat("##bloomintensity", mContext->mSettings.BloomIntensity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);
				ImGui::Text("Bloom Threshold");
				ImGuiHelpers::ManualDragFloat("##bloomtreshold", mContext->mSettings.BloomThreshold, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);
				ImGui::Checkbox("Dynamic IBL", &mContext->mSettings.DynamicIBL);
				if (ImGui::Checkbox("Planet backface culling", &mContext->mSettings.BackfaceCulling))
					mContext->mSettings.IsDirty = true;
				if (ImGui::Checkbox("Planet frustum culling", &mContext->mSettings.FrustumCulling))
					mContext->mSettings.IsDirty = true;
				ImGui::Checkbox("Render Colliders", &mContext->mSettings.RenderColliders);
				ImGui::Checkbox("Render UI", &mContext->mSettings.RenderUI);

				ImGui::Text("Physics slow motion");
				ImGui::SliderInt("##physicsslowmotion", &mContext->mSettings.PhysicSlowmotion, 1, 30);

				ImGui::Text("Sun Frustum Ortho Size");
				ImGuiHelpers::ManualDragFloat("##sunlightdistance", mContext->mSettings.SunFrustumOrthoSize, mWindow, activeDragArea, 10.0f, ImVec2{ 255.0f, 20.0f }, "%.1f", 50.0f, 10000.0f);

				ImGui::Text("God Rays Exposure");
				ImGuiHelpers::ManualDragFloat("##godraysexposure", mContext->mSettings.GodRaysExposure, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);
				ImGui::Text("God Rays Decay");
				ImGuiHelpers::ManualDragFloat("##godraysdecay", mContext->mSettings.GodRaysDecay, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);
				ImGui::Text("God Rays Density");
				ImGuiHelpers::ManualDragFloat("##godraysdensity", mContext->mSettings.GodRaysDensity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 5.0f);
				ImGui::Text("God Rays Weight");
				ImGuiHelpers::ManualDragFloat("##godraysweight", mContext->mSettings.GodRaysWeight, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);
			}

			ImGui::End();
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
	}

}