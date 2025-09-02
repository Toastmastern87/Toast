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

		ImGui::Begin(ICON_TOASTER_CLOUD" Environment");

		ImGui::Text("Sun Disc Toggle");
		ImGui::Checkbox("##SunDiscToggle", &environment.SunDiscToggle);

		ImGui::Text("Sun Intensity");
		ImGui::DragFloat("##SunIntensity", &environment.SunIntensity, 0.1f, 0.0f, FLT_MAX, "%.1f");

		ImGui::Text("Sun Disc Radius");
		ImGui::DragFloat("##SunDiscRadius", &environment.SunDiscRadius, 0.000001f, 0.0f, FLT_MAX, "%.6f");

		ImGui::Text("Sun Edge Softness");
		ImGui::DragFloat("##SunEdgeSoftness", &environment.SunEdgeSoftness, 0.0001f, 0.0f, FLT_MAX, "%.4f");

		ImGui::Text("Sun Glow Intensity");
		ImGui::DragFloat("##SunGlowIntensity", &environment.SunGlowIntensity, 0.1f, 0.0f, FLT_MAX, "%.1f");

		ImGui::Text("Sun Glow Size");
		ImGui::DragFloat("##SunGlowSize", &environment.SunGlowSize, 0.000001f, 0.0f, FLT_MAX, "%.6f");

		ImGui::End();
	}

}