#pragma once

#include "PlanetPanel.h"

#include "Toast/Core/Log.h"

#include "imgui/imgui.h"

namespace Toast {

	void PlanetPanel::OnImGuiRender()
	{
		ImGuiIO& io = ImGui::GetIO();

		const float popupWidth = io.DisplaySize.x * 0.3f;
		const float popupHeight = 200.0f;
		ImVec2 popupSize(popupWidth, popupHeight);
		ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.25f); 

		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(popupSize, ImGuiCond_Always);

		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		if (ImGui::BeginPopup("Planet"))
		{
			const float titleBarHeight = 36.0f;
			const float buttonSize = 24.0f;

			ImVec2 windowPos = ImGui::GetWindowPos();
			ImVec2 windowSize = ImGui::GetWindowSize();

			// Title bar background
			ImVec2 titleBarMin = ImGui::GetWindowPos();
			ImVec2 titleBarMax = ImVec2(titleBarMin.x + ImGui::GetWindowSize().x, titleBarMin.y + titleBarHeight);
			ImU32 titleBarColor = ImGui::GetColorU32(ImGuiCol_Header);
			ImGui::GetWindowDrawList()->AddRectFilled(titleBarMin, titleBarMax, titleBarColor);

			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 12.0f, windowPos.y + 8.0f));
			ImGui::PushFont(io.Fonts->Fonts[3]);
			ImGui::TextUnformatted("Planet");
			ImGui::PopFont();

			// Close button
			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + windowSize.x - buttonSize - 6.0f, windowPos.y + 6.0f));
			if (ImGui::Button("X", ImVec2(buttonSize, buttonSize)))
				ImGui::CloseCurrentPopup();

			// Push content below title bar
			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 10.0f, windowPos.y + titleBarHeight + 6.0f));
			ImGui::BeginGroup();

			ImGui::Text("This is a popup with a custom title bar.");

			ImGui::EndGroup();
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		ImGui::PopStyleColor();
	}
}