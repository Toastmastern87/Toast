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
		ImGui::Begin(ICON_TOASTER_CLOUD" Environment");

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, 200.0f);
		ImGui::Text("Skybox LOD");
		ImGui::NextColumn();
		ImGui::SliderFloat("##SkyboxLOD", &mContext->GetSkyboxLod(), 0.0f, 11.0f, "%.2f");
		ImGui::PushItemWidth(-1);

		ImGui::End();
	}

}