#include "EnvironmentalPanel.h"

#include "../FontAwesome.h"

#include "Toast/Utils/PlatformUtils.h"

#include "imgui/imgui.h"

namespace Toast {

	EnvironmentalPanel::EnvironmentalPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void EnvironmentalPanel::SetContext(const Ref<Scene>& context)
	{
		mContext = context;
	}

	void EnvironmentalPanel::OnImGuiRender()
	{
		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::Begin(ICON_TOASTER_CLOUD" Environment");

		ImGui::Text("Skybox LOD");
		ImGui::SliderFloat("##SkyboxLOD", &mContext->GetSkyboxLod(), 0.0f, 11.0f, "%.2f");

		ImGui::End();
	}

}