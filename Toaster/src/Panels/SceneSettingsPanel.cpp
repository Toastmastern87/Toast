#include "SceneSettingsPanel.h"

#include "../FontAwesome.h"

#include "Toast/ImGui/ImGuiHelpers.h"
#include "Toast/Physics/PhysicsEngine.h"

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
					const char* items[] = { "None", "G-Buffer Positions", "G-Buffer Normals", "G-Buffer Albedo/Metallic", "Roughness", "Lighting Pass Output", "Atmospheric Scattering Output", "SSAO", "SSAO Blur", "Bloom", "Bloom Half", "Bloom Quarter", "Bloom Final", "SkyViewLUT", "Planet Materials Debug"};
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
					ImGui::Text("Frustum Culling Margin");
					ImGuiHelpers::ManualDragFloat("##FrustumCullingMargin", mContext->mSettings.FrustumCullingMargin, mWindow, activeDragArea, 1.0f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 5000.0f);
					ImGui::Text("Directional Lightning Gain");
					ImGuiHelpers::ManualDragFloat("##dirlightgain", mContext->mSettings.DirectionalLightningGain, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, FLT_MAX);
					ImGui::Checkbox("Activate Shadows", &mContext->mSettings.Shadows.Active);
					if (mContext->mSettings.Shadows.Active)
					{
						if (ImGui::CollapsingHeader("Shadows"))
						{
							ImGui::Indent();

							ImGui::Text("Cascade Count");
							int temp = mContext->mSettings.Shadows.CascadeCount;
							if (ImGui::SliderInt("##CascadeCount", &temp, 1, 4))
							{
								mContext->mSettings.Shadows.CascadeCount = temp;
								mContext->mSettings.Shadows.IsDirty = true;
							}

							ImGui::Text("Distance");
							if (ImGuiHelpers::ManualDragFloat("##ShadowDistance", mContext->mSettings.Shadows.ShadowDistance, mWindow, activeDragArea, 10.0f, ImVec2{ 255.0f, 20.0f }, "%.0f", 0.0f, 25000.0f))
								mContext->mSettings.Shadows.IsDirty = true;
							ImGui::Text("Lambda");
							if (ImGuiHelpers::ManualDragFloat("##ShadowLambda", mContext->mSettings.Shadows.Lambda, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 1.0f))
								mContext->mSettings.Shadows.IsDirty = true;
							ImGui::Text("Constant bias");
							ImGuiHelpers::ManualDragFloat("##ConstantBias", mContext->mSettings.Shadows.ConstantBias, mWindow, activeDragArea, 0.0001f, ImVec2{ 255.0f, 20.0f }, "%.4f", 0.0f, 0.01f);
							ImGui::Text("Slope-scaled bias");
							ImGuiHelpers::ManualDragFloat("##SlopeScaledBias", mContext->mSettings.Shadows.SlopeScaledBias, mWindow, activeDragArea, 0.0001f, ImVec2{ 255.0f, 20.0f }, "%.4f", 0.0f, 0.02f);

							ImGui::Unindent();
						}
					}
					ImGui::Checkbox("SSAO", &mContext->mSettings.SSAO);
					ImGui::Checkbox("SSAODebugging", &mContext->mSettings.SSAODebugging);
					ImGui::Text("SSAO Radius");
					ImGuiHelpers::ManualDragFloat("##ssaoradius", mContext->mSettings.SSAORadius, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 50.0f);
					ImGui::Text("SSAO bias");
					ImGuiHelpers::ManualDragFloat("##ssaobias", mContext->mSettings.SSAObias, mWindow, activeDragArea, 0.001f, ImVec2{ 255.0f, 20.0f }, "%.4f", -1.0f, 1.0f);

					if (ImGui::CollapsingHeader("Physics"))
					{
						auto& physicsSettings = mContext->GetPhysicsEngine()->GetSettings();

						ImGui::Indent();

						ImGui::Text("Sub steps");
						ImGui::SliderInt("##Substeps", &physicsSettings.StepsPerUpdate, 1, 10);
						ImGui::Text("FPS Target");
						ImGui::SliderInt("##FPSTarget", &physicsSettings.FPSTarget, 1, 120);
						ImGui::Text("Slow motion factor");
						ImGui::SliderInt("##physicsslowmotion", &physicsSettings.SlowDown, 1, 30);
						ImGui::Text("Max Angular Velocity");
						ImGuiHelpers::ManualDragFloat("##MaxAngularVelocity", physicsSettings.MaxAngularVelocity, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.1f", 0.0f, 50.0f);

						ImGui::Unindent();
					}

					ImGui::Checkbox("Bloom", &mContext->mSettings.Bloom.Enabled);
					if (ImGui::CollapsingHeader("Bloom Settings") && mContext->mSettings.Bloom.Enabled)
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
					ImGui::Checkbox("Render Colliders", &mContext->mSettings.RenderColliders);
					ImGui::Checkbox("Render UI", &mContext->mSettings.RenderUI);

					ImGui::Text("Sun Frustum Ortho Size");
					ImGuiHelpers::ManualDragFloat("##sunlightdistance", mContext->mSettings.SunFrustumOrthoSize, mWindow, activeDragArea, 10.0f, ImVec2{ 255.0f, 20.0f }, "%.1f", 50.0f, 10000.0f);

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();

					if (ImGui::CollapsingHeader("Entity Hovering"))
					{
						ImGui::Text("Hover Tint");
						ImGui::ColorEdit4("##HoverTintColor", &mContext->mSettings.HoverTintColor.x, ImGuiColorEditFlags_AlphaBar);
					}

					if (ImGui::CollapsingHeader("Selection Outline"))
					{
						ImGui::Indent();

						auto& outline = mContext->mSettings.Outline;

						ImGui::Text("Color");
						ImGui::ColorEdit4("##OutlineColor", &outline.Color.x);

						ImGui::Text("Thickness (px)");
						ImGuiHelpers::ManualDragFloat("##OutlineThickness", outline.Thickness, mWindow, activeDragArea, 0.1f, ImVec2{ 255.0f, 20.0f }, "%.1f", 0.0f, 16.0f);

						ImGui::Text("Softness");
						ImGuiHelpers::ManualDragFloat("##OutlineSoftness", outline.Softness, mWindow, activeDragArea, 0.05f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 8.0f);

						ImGui::Text("Pulse Speed");
						ImGuiHelpers::ManualDragFloat("##OutlinePulseSpeed", outline.PulseSpeed, mWindow, activeDragArea, 0.05f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 10.0f);

						ImGui::Checkbox("Visible through occluders (X-Ray)", &outline.XRay);

						ImGui::Unindent();
					}

					if (ImGui::CollapsingHeader("God Rays"))
					{
						ImGui::Indent();

						ImGui::Text("Exposure");
						ImGuiHelpers::ManualDragFloat("##godraysexposure", mContext->mSettings.GodRays.Exposure, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);

						ImGui::Text("Decay");
						ImGuiHelpers::ManualDragFloat("##godraysdecay", mContext->mSettings.GodRays.Decay, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);

						ImGui::Text("Density");
						ImGuiHelpers::ManualDragFloat("##godraysdensity", mContext->mSettings.GodRays.Density, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 50.0f);

						ImGui::Text("Weight");
						ImGuiHelpers::ManualDragFloat("##godraysweight", mContext->mSettings.GodRays.Weight, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 2.0f);

						ImGui::Text("kHalo");
						ImGuiHelpers::ManualDragFloat("##godraysKHalo", mContext->mSettings.GodRays.KHalo, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 1.0f);

						ImGui::Text("Halo Power");
						ImGuiHelpers::ManualDragFloat("##godrayshalopower", mContext->mSettings.GodRays.HaloPower, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.0f, 5.0f);

						ImGui::Text("Fog Range Meters");
						ImGuiHelpers::ManualDragFloat("##godraysFogRangeMeters", mContext->mSettings.GodRays.FogRangeMeters, mWindow, activeDragArea, 100.0f, ImVec2{ 255.0f, 20.0f }, "%.0f", 0.0f, 200000.0f);

						ImGui::Unindent();
					}

					if (ImGui::CollapsingHeader("Exposure"))
					{
						ImGui::Indent();

						// Daytime EVs (clear labels above each control)
						ImGui::TextUnformatted("Surface Day");
						ImGuiHelpers::ManualDragFloat("##EVSurfaceDay", mContext->mSettings.Exposure.EVSurfaceDay, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::TextUnformatted("Space Day");
						ImGuiHelpers::ManualDragFloat("##EVSpaceDay", mContext->mSettings.Exposure.EVSpaceDay, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::TextUnformatted("Surface Night");
						ImGuiHelpers::ManualDragFloat("##EVSurfaceNight", mContext->mSettings.Exposure.EVSurfaceNight, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::TextUnformatted("Space Night");
						ImGuiHelpers::ManualDragFloat("##EVSpaceNight", mContext->mSettings.Exposure.EVSpaceNight, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -10.0f, 10.0f);

						ImGui::Separator();

						ImGui::TextUnformatted("Altitude Blend (0..1)");
						ImGuiHelpers::ManualDragFloat2("##AltFadeFrac", mContext->mSettings.Exposure.AltFadeFrac, 0.02f, 0.0f, mWindow, activeDragArea, "%.3f");

						ImGui::TextUnformatted("Sun Fade (0..1)");
						ImGuiHelpers::ManualDragFloat2("##SunFadeDeg", mContext->mSettings.Exposure.SunFadeDeg, 0.02f, 0.0f, mWindow, activeDragArea, "%.3f");

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