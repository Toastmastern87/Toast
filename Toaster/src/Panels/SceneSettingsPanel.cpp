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

			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 10.0f, windowPos.y + titleBarHeight + 6.0f));          // place child just under header
			if (ImGui::BeginChild("SceneSettingsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) 
			{
				ImGui::Spacing();
				ImGui::Indent(10.0f);

				if (mContext)
				{
					const char* items[] = { "None", "G-Buffer Positions", "G-Buffer Normals", "G-Buffer Albedo/Metallic", "Roughness", "Lighting Pass Output", "Atmospheric Scattering Output", "SSAO", "SSAO Blur", "Bloom", "Bloom Half", "Bloom Quarter", "Bloom Final", "SkyViewLUT" };
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
					ImGui::Text("Directional Lightning Gain");
					ImGuiHelpers::ManualDragFloat("##dirlightgain", mContext->mSettings.DirectionalLightningGain, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 50.0f);
					ImGui::Checkbox("Show sun light frustum", &mContext->mSettings.SunLightFrustum);
					ImGui::Checkbox("Shadows", &mContext->mSettings.Shadows);
					ImGui::Checkbox("SSAO", &mContext->mSettings.SSAO);
					ImGui::Checkbox("SSAODebugging", &mContext->mSettings.SSAODebugging);
					ImGui::Text("SSAO Radius");
					ImGuiHelpers::ManualDragFloat("##ssaoradius", mContext->mSettings.SSAORadius, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 50.0f);
					ImGui::Text("SSAO bias");
					ImGuiHelpers::ManualDragFloat("##ssaobias", mContext->mSettings.SSAObias, mWindow, activeDragArea, 0.001f, ImVec2{ 255.0f, 20.0f }, "%.4f", -1.0f, 1.0f);
					ImGui::Checkbox("Bloom", &mContext->mSettings.Bloom.Enabled);
					if (ImGui::CollapsingHeader("Bloom Settings", ImGuiTreeNodeFlags_DefaultOpen) && mContext->mSettings.Bloom.Enabled)
					{
						ImGui::Indent();

						ImGui::Text("Sun from Surface Threshold");
						ImGuiHelpers::ManualDragFloat("##SunSurfaceThreshold ", mContext->mSettings.Bloom.SunSurfaceThreshold, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);
						ImGui::Text("Sun from Surface Intensity");
						ImGuiHelpers::ManualDragFloat("##sunsurfaceintensity", mContext->mSettings.Bloom.SunSurfaceIntensity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);

						ImGui::Text("Sun from Space Threshold");
						ImGuiHelpers::ManualDragFloat("##SunSpaceThreshold ", mContext->mSettings.Bloom.SunSpaceThreshold, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);
						ImGui::Text("Sun from Space Intensity");
						ImGuiHelpers::ManualDragFloat("##sunSpaceintensity", mContext->mSettings.Bloom.SunSpaceIntensity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);

						ImGui::Text("Sky from Surface Threshold");
						ImGuiHelpers::ManualDragFloat("##SkySurfaceThreshold ", mContext->mSettings.Bloom.SkySurfaceThreshold, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);
						ImGui::Text("Sky from Surface Intensity");
						ImGuiHelpers::ManualDragFloat("##Skynsurfaceintensity", mContext->mSettings.Bloom.SkySurfaceIntensity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);

						ImGui::Text("Sky from Space Threshold");
						ImGuiHelpers::ManualDragFloat("##SkySpaceThreshold ", mContext->mSettings.Bloom.SkySpaceThreshold, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);
						ImGui::Text("Sky from Space Intensity");
						ImGuiHelpers::ManualDragFloat("##SkySpaceIntensity", mContext->mSettings.Bloom.SkySpaceIntensity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);

						ImGui::Text("Geometry Threshold");
						ImGuiHelpers::ManualDragFloat("##Geometrytreshold", mContext->mSettings.Bloom.GeometryThreshold, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 25.0f);
						ImGui::Text("Geometry Intensity");
						ImGuiHelpers::ManualDragFloat("##GeometryIntensity", mContext->mSettings.Bloom.GeometryIntensity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);

						ImGui::Text("Sun Radius");
						ImGuiHelpers::ManualDragFloat("##SunRadius", mContext->mSettings.Bloom.SunRadius, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);
						ImGui::Text("Sky Surface Radius");
						ImGuiHelpers::ManualDragFloat("##SkySurfaceRadius", mContext->mSettings.Bloom.SkySurfaceRadius, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);
						ImGui::Text("Sky Space Radius");
						ImGuiHelpers::ManualDragFloat("##SkySpaceRadius", mContext->mSettings.Bloom.SkySpaceRadius, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);
						ImGui::Text("Soft Knee");
						ImGuiHelpers::ManualDragFloat("##SoftKnee", mContext->mSettings.Bloom.SoftKnee, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 1.0f);
						ImGui::Text("Saturation Clamp");
						ImGuiHelpers::ManualDragFloat("##SaturationClamp", mContext->mSettings.Bloom.SaturationClamp, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 1.0f);

						ImGui::Unindent();
					}
					ImGui::Spacing();
					
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

					if (ImGui::CollapsingHeader("Exposure Settings", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Indent();

						// Daytime EVs (clear labels above each control)
						ImGui::TextUnformatted("Geometry — Surface");
						ImGuiHelpers::ManualDragFloat("##EVGeometrySurface",
							mContext->mSettings.Exposure.EVGeometrySurface,
							mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::TextUnformatted("Geometry — Space (Orbit)");
						ImGuiHelpers::ManualDragFloat("##EVGeometrySpace",
							mContext->mSettings.Exposure.EVGeometrySpace,
							mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::TextUnformatted("Sky — Surface");
						ImGuiHelpers::ManualDragFloat("##EVSkySurface",
							mContext->mSettings.Exposure.EVSkySurface,
							mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::TextUnformatted("Sky — Space (Orbit)");
						ImGuiHelpers::ManualDragFloat("##EVSkySpace", mContext->mSettings.Exposure.EVSkySpace,
							mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::Separator();

						// Night EV (single)
						ImGui::TextUnformatted("Night (Geometry)");
						ImGuiHelpers::ManualDragFloat("##EVGeometryNight",mContext->mSettings.Exposure.EVGeometryNight,	mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						// Night EV (single)
						ImGui::TextUnformatted("Night Sky (From Surface)");
						ImGuiHelpers::ManualDragFloat("##EVSkySurfaceNight", mContext->mSettings.Exposure.EVSkySurfaceNight, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::TextUnformatted("Night Sky (From Space)");
						ImGuiHelpers::ManualDragFloat("##EVSkySpaceNight", mContext->mSettings.Exposure.EVSkySpaceNight, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::Separator();

						ImGui::TextUnformatted("Altitude Blend Start (0..1)");
						ImGuiHelpers::ManualDragFloat("##AltFadeStartFrac",
							mContext->mSettings.Exposure.AltFadeStartFrac,
							mWindow, activeDragArea, 0.005f, ImVec2{ 255,20 }, "%.3f", 0.0f, 1.0f);

						ImGui::TextUnformatted("Altitude Blend End (0..1)");
						ImGuiHelpers::ManualDragFloat("##AltFadeEndFrac",
							mContext->mSettings.Exposure.AltFadeEndFrac,
							mWindow, activeDragArea, 0.005f, ImVec2{ 255,20 }, "%.3f", 0.0f, 1.0f);

						ImGui::TextUnformatted("Sun Fade Start (deg)");
						ImGuiHelpers::ManualDragFloat("##SunFadeStartDeg",
							mContext->mSettings.Exposure.SunFadeStartDeg,
							mWindow, activeDragArea, 0.1f, ImVec2{ 255,20 }, "%.1f", -30.0f, 30.0f);

						ImGui::TextUnformatted("Sun Fade End (deg)");
						ImGuiHelpers::ManualDragFloat("##SunFadeEndDeg",
							mContext->mSettings.Exposure.SunFadeEndDeg,
							mWindow, activeDragArea, 0.1f, ImVec2{ 255,20 }, "%.1f", -30.0f, 30.0f);

						ImGui::Unindent();
					}
				}

				ImGui::Spacing();
			}
			ImGui::EndChild();

			ImGui::End();
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
	}

}