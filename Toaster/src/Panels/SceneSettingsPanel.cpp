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
					const char* items[] = { "None", "G-Buffer Positions", "G-Buffer Normals", "G-Buffer Albedo/Metallic", "Roughness", "Lighting Pass Output", "Atmospheric Scattering Output", "SSAO", "SSAO Blur", "Bloom", "Bloom Final", "SkyViewLUT", "Planet Materials Debug" };
					int currentOverlay = static_cast<int>(mContext->mSettings.RenderOverlaySetting);

					ImGui::Text("Render Overlay");
					ImGui::SameLine();
					if (ImGui::Combo("##renderoverlayid", &currentOverlay, items, IM_ARRAYSIZE(items)))
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
					ImGui::Checkbox("Show UI Bounds", &mContext->mSettings.ShowUIBounds);
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

						ImGui::Text("Intensity");
						ImGuiHelpers::ManualDragFloat("##BloomIntensity", mContext->mSettings.Bloom.Intensity, mWindow, activeDragArea, 0.001f, ImVec2{ 255.0f, 20.0f }, "%.3f", 0.0f, 1.0f);

						ImGui::Text("Falloff");
						ImGuiHelpers::ManualDragFloat("##BloomFalloff", mContext->mSettings.Bloom.Falloff, mWindow, activeDragArea, 0.01f, ImVec2{ 255.0f, 20.0f }, "%.2f", 0.2f, 1.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("How fast the glow fades with distance.\n0.5 = camera lens, 1.0 = human eye (wider, softer).");

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
						auto& exp = mContext->mSettings.Exposure;

						ImGui::Checkbox("Lock EV", &mContext->mSettings.Exposure.LockEV);
						if (mContext->mSettings.Exposure.LockEV)
						{
							ImGui::TextUnformatted("Locked EV");
							ImGuiHelpers::ManualDragFloat("##LockedEV", mContext->mSettings.Exposure.LockedEV, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -16.0f, 8.0f);
						}

						ImGui::Separator();

						ImGui::TextUnformatted("Compensation (stops)");
						ImGuiHelpers::ManualDragFloat("##AECompensation", exp.Compensation, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", -5.0f, 5.0f);
						ImGui::TextUnformatted("Min EV");
						ImGuiHelpers::ManualDragFloat("##AEMinEV", exp.MinEV, mWindow, activeDragArea, 0.05f, ImVec2{ 255,20 }, "%.2f", -16.0f, 8.0f);
						ImGui::TextUnformatted("Max EV");
						ImGuiHelpers::ManualDragFloat("##AEMaxEV", exp.MaxEV, mWindow, activeDragArea, 0.05f, ImVec2{ 255,20 }, "%.2f", -16.0f, 8.0f);
						ImGui::TextUnformatted("Min EV (orbit)");
						ImGuiHelpers::ManualDragFloat("##AEOrbitMinEV", exp.OrbitMinEV, mWindow, activeDragArea, 0.05f, ImVec2{ 255,20 }, "%.2f", -16.0f, 8.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Min EV when the camera is above the atmosphere (blends in high up).\nHigher = the night side stays dark from orbit.\nEqual to Min EV = off.");

						ImGui::TextUnformatted("Low Percentile (%)");
						ImGuiHelpers::ManualDragFloat("##AELowPercentile", exp.LowPercentile, mWindow, activeDragArea, 0.5f, ImVec2{ 255,20 }, "%.1f", 0.0f, 98.0f);
						ImGui::TextUnformatted("High Percentile (%)");
						ImGuiHelpers::ManualDragFloat("##AEHighPercentile", exp.HighPercentile, mWindow, activeDragArea, 0.5f, ImVec2{ 255,20 }, "%.1f", 2.0f, 100.0f);
						ImGui::TextUnformatted("Highlight Percentile (%)");
						ImGuiHelpers::ManualDragFloat("##AEHighlightPercentile", exp.HighlightPercentile, mWindow, activeDragArea, 0.05f, ImVec2{ 255,20 }, "%.2f", 90.0f, 100.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("The brightest (100 - this) %% of the frame is protected from going white.\n99.5 = about the sun's glow within ~2 degrees.");

						ImGui::TextUnformatted("Highlight Target");
						ImGuiHelpers::ManualDragFloat("##AEHighlightTarget", exp.HighlightTarget, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", 0.1f, 16.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("How bright the protected highlights may get (before the tone curve).\n0.6 keeps a blue sunset blue; higher = brighter rest of frame, whiter highlights.\n16 = off.");

						ImGui::TextUnformatted("Highlight Target (orbit)");
						ImGuiHelpers::ManualDragFloat("##AEOrbitHighlightTarget", exp.OrbitHighlightTarget, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", 0.1f, 16.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Highlight Target when the camera is above the atmosphere (blends in high up).\nLower = the lit limb and the sun's surroundings keep their gradient.");

						ImGui::TextUnformatted("Adaptation Knee EV");
						ImGuiHelpers::ManualDragFloat("##AEAdaptationKneeEV", exp.AdaptationKneeEV, mWindow, activeDragArea, 0.05f, ImVec2{ 255,20 }, "%.2f", -16.0f, 8.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Down to this exposure the camera fully adapts (daylight).\nBelow it, Dim-Light Adaptation decides how much.");

						ImGui::TextUnformatted("Dim-Light Adaptation");
						ImGuiHelpers::ManualDragFloat("##AEDimLightAdaptation", exp.DimLightAdaptation, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", 0.0f, 1.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Below the knee: how much of the drop in light the exposure follows.\n1 = fully (dusk looks like day), lower = dusk stays darker, like the eye.");

						ImGui::TextUnformatted("Local Highlight Contrast");
						ImGuiHelpers::ManualDragFloat("##AELocalHighlightContrast", exp.LocalHighlightContrast, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", 0.0f, 1.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Local tone mapping: < 1 compresses bright areas (the glow around the sun, a sunset sky)\ntowards middle grey, keeping their detail. 1 = off.");

						ImGui::TextUnformatted("Local Shadow Contrast");
						ImGuiHelpers::ManualDragFloat("##AELocalShadowContrast", exp.LocalShadowContrast, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", 0.0f, 1.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Local tone mapping: < 1 lifts dark areas towards middle grey. 1 = off.");

						ImGui::TextUnformatted("Local Detail Strength");
						ImGuiHelpers::ManualDragFloat("##AELocalDetailStrength", exp.LocalDetailStrength, mWindow, activeDragArea, 0.01f, ImVec2{ 255,20 }, "%.2f", 0.0f, 2.0f);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("How much of each pixel's difference from its area is kept. 1 = all of it.");

						ImGui::TextUnformatted("Speed To Bright (1/s)");
						ImGuiHelpers::ManualDragFloat("##AESpeedToBright", exp.SpeedToBright, mWindow, activeDragArea, 0.05f, ImVec2{ 255,20 }, "%.2f", 0.05f, 20.0f);
						ImGui::TextUnformatted("Speed To Dark (1/s)");
						ImGuiHelpers::ManualDragFloat("##AESpeedToDark", exp.SpeedToDark, mWindow, activeDragArea, 0.05f, ImVec2{ 255,20 }, "%.2f", 0.05f, 20.0f);

						if (ImGui::TreeNode("Histogram Range"))
						{
							ImGui::TextUnformatted("Min (log2 luminance)");
							ImGuiHelpers::ManualDragFloat("##AEHistogramMinLog2", exp.HistogramMinLog2, mWindow, activeDragArea, 0.1f, ImVec2{ 255,20 }, "%.1f", -24.0f, 0.0f);
							ImGui::TextUnformatted("Range (stops)");
							ImGuiHelpers::ManualDragFloat("##AEHistogramRange", exp.HistogramRange, mWindow, activeDragArea, 0.1f, ImVec2{ 255,20 }, "%.1f", 4.0f, 32.0f);
							ImGui::TreePop();
						}

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