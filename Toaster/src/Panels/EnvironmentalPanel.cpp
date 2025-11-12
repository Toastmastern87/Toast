#include "EnvironmentalPanel.h"

#include "../FontAwesome.h"

#include "Toast/Utils/PlatformUtils.h"

#include "imgui/imgui.h"

namespace Toast {

	EnvironmentalPanel::EnvironmentalPanel(Scene* context)
	{
		SetContext(context);
	}

	void EnvironmentalPanel::SetContext(Scene* context)
	{
		mContext = context;
	}

	void EnvironmentalPanel::OnImGuiRender()
	{
		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		Scene::Environment& environment = mContext->GetEnvirontment();

		ImGui::Begin(ICON_TOASTER_CLOUD " Environment");

		// ---------------- Sun ----------------
		ImGui::Separator();
		ImGui::Text("Lightning Gains");

		ImGui::Text("Diffuse IBL Gain");
		ImGui::DragFloat("##DiffuseIBLGain", &environment.DiffuseIBLGain, 0.1f, 0.0f, FLT_MAX, "%.1f");

		ImGui::Text("Specular IBL Gain");
		ImGui::DragFloat("##SpecularIBLGain", &environment.SpecularIBLGain, 0.1f, 0.0f, FLT_MAX, "%.1f");

		// ---------------- Sun ----------------
		ImGui::Separator();
		ImGui::Text("Sun");

		ImGui::Text("Sun Disc Toggle");
		ImGui::Checkbox("##SunDiscToggle", &environment.SunDiscToggle);

		ImGui::Text("Sun Intensity");
		ImGui::DragFloat("##SunIntensity", &environment.SunIntensity, 0.1f, 0.0f, FLT_MAX, "%.1f");

		ImGui::Text("Sun Disc Radius (rad)");
		ImGui::DragFloat("##SunDiscRadius", &environment.SunDiscRadius, 0.000001f, 0.0f, FLT_MAX, "%.6f");

		ImGui::Text("Sun Edge Softness (rad)");
		ImGui::DragFloat("##SunEdgeSoftness", &environment.SunEdgeSoftness, 0.0001f, 0.0f, FLT_MAX, "%.4f");

		ImGui::Text("Sun White (RGB)");
		ImGui::ColorEdit3("##SunWhite", &environment.SunWhite.x);

		ImGui::Text("Warm Tint (RGB)");
		ImGui::ColorEdit3("##WarmTint", &environment.WarmTint.x);

		ImGui::Text("Space Disc Brightness Scale");
		ImGui::DragFloat("##SpaceDiscBrightnessScale", &environment.SpaceDiscBrightnessScale, 0.01f, 0.0f, 4.0f, "%.2f");

		// ------------- Atmospheric Halo (in-air) -------------
		ImGui::Separator();
		ImGui::Text("Atmospheric Halo");

		ImGui::Text("Air Halo Intensity");
		ImGui::DragFloat("##AirHaloIntensity", &environment.AirHaloIntensity, 0.01f, 0.0f, 1.0f, "%.2f");

		ImGui::Text("Air Halo Start (Frac)");
		ImGui::DragFloat("##AirHaloStartFrac", &environment.AirHaloStartFrac, 0.01f, 0.0f, 1.0f, "%.2f");

		ImGui::Text("Air Halo Falloff Pow");
		ImGui::DragFloat("##AirHaloFalloffPow", &environment.AirHaloFalloffPow, 0.01f, 0.1f, 3.0f, "%.2f");

		ImGui::Text("Horizon Refraction (deg)");
		ImGui::DragFloat("##HorizonRefractionDeg", &environment.HorizonRefractionDeg, 0.01f, 0.0f, 2.0f, "%.2f");

		ImGui::Text("Twilight Blend (deg)");
		ImGui::DragFloat("##TwilightBlendDeg", &environment.TwilightBlendDeg, 0.01f, 0.0f, 5.0f, "%.2f");

		// ---------------- Space Halo ----------------
		ImGui::Separator(); 
		ImGui::Text("Space Halo");

		ImGui::Text("Space Halo Width (deg)");
		ImGui::DragFloat("##SpaceHaloWidthDeg", &environment.SpaceHaloWidthDeg, 0.01f, 0.1f, 5.0f, "%.2f");

		ImGui::Text("Space Halo Intensity");
		ImGui::DragFloat("##SpaceHaloIntensity", &environment.SpaceHaloIntensity, 0.005f, 0.0f, 0.5f, "%.3f");

		ImGui::Text("Space Halo Cutoff (deg)");
		ImGui::DragFloat("##SpaceHaloCutoffDeg", &environment.SpaceHaloCutoffDeg, 0.1f, 0.0f, 30.0f, "%.1f");

		// ---------------- Stars ----------------
		ImGui::Separator();
		ImGui::Text("Stars");

		ImGui::Text("Star Nits");
		ImGui::DragFloat("##StarNits", &environment.StarNits, 10.0f, 0.0f, FLT_MAX, "%.0f");

		ImGui::Text("Twilight Start (deg)");
		ImGui::DragFloat("##TwilightStartDeg", &environment.TwilightStartDeg, 0.1f, -60.0f, 60.0f, "%.1f");

		ImGui::Text("Twilight End (deg)");
		ImGui::DragFloat("##TwilightEndDeg", &environment.TwilightEndDeg, 0.1f, -60.0f, 60.0f, "%.1f");

		ImGui::Text("Space Fade Start");
		ImGui::DragFloat("##SpaceFadeStart", &environment.SpaceFadeStart, 0.01f, 0.0f, 1.0f, "%.2f");

		ImGui::Text("Space Fade End");
		ImGui::DragFloat("##SpaceFadeEnd", &environment.SpaceFadeEnd, 0.01f, 0.0f, 1.0f, "%.2f");

		// --------------- Night Ambient ---------------
		ImGui::Separator();
		ImGui::Text("Night Ambient");
		ImGui::Text("Night Ambient Light (x0.01)");
		ImGui::ColorEdit3("##NightAmbientLight", &environment.NightAmbient.x);

		ImGui::End();
	}

}