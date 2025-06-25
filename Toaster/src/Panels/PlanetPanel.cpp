#pragma once

#include "PlanetPanel.h"

#include "Toast/Core/Log.h"

#include "imgui/imgui.h"

namespace Toast {

	void PlanetPanel::SetContext(Scene* context)
	{
		mContext = context;
	}

	void PlanetPanel::OnImGuiRender()
	{
		ImGuiIO& io = ImGui::GetIO();

		const float popupWidth = io.DisplaySize.x * 0.15f;
		const float popupHeight = 300.0f;
		ImVec2 popupSize(popupWidth, popupHeight);
		ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.25f); 

		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(popupSize, ImGuiCond_Always);

		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		const ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

		if (ImGui::BeginPopupModal("Planet", nullptr, popupFlags))
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
			if (ImGui::Button("X##PlanetClose", ImVec2(buttonSize, buttonSize)))
				ImGui::CloseCurrentPopup();

			// Push content below title bar
			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 10.0f, windowPos.y + titleBarHeight + 6.0f));
			
			ImGui::Spacing(); 

			ImGui::Indent(10.0f);

			// section header
			ImGui::PushFont(io.Fonts->Fonts[4]);     
			ImGui::TextUnformatted("Planet Clip Data");
			ImGui::PopFont();

			ImGui::PushID("PlanetPopupControls");
			ImGui::BeginGroup();

			static int gridSizes[] = { 33, 65, 129, 257, 513 };
			static int currentGridSize = 129;
			static int currentLOD = 5;

			// Create a 2-column table for label + control layout
			if (ImGui::BeginTable("PlanetTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.4);
				ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.6);

				// === Grid Size Row ===
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Grid Size");

				ImGui::TableSetColumnIndex(1);

				auto  padX = ImGui::GetStyle().CellPadding.x;
				float colW = ImGui::GetColumnWidth();               // full width of this column
				float fullW = colW - padX * 2.0f;

				ImGui::SetNextItemWidth(fullW);
				std::string gridLabel = std::to_string(PlanetSystem::sGridSize);          // keep it alive
				if (ImGui::BeginCombo("##GridSize", gridLabel.c_str()))
				{
					for (int i = 0; i < IM_ARRAYSIZE(gridSizes); ++i)
					{
						bool selected = (currentGridSize == gridSizes[i]);
						if (ImGui::Selectable(std::to_string(gridSizes[i]).c_str(), selected))
							PlanetSystem::sGridSize = gridSizes[i];
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				// === LOD Row ===
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Levels of Detail");

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(fullW);
				ImGui::SliderInt("##LOD", &PlanetSystem::sNumLevels, 1, 30);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(1);

				const float btnW = 80.0f;                    
				float indent = fullW - btnW; 
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

				if (ImGui::Button("Apply", ImVec2(btnW, 0)))
				{
					if (PlanetSystem::sNumLevels != 0 && PlanetSystem::sGridSize != 0)
					{
						PlanetSystem::RebuildGrid();

						SceneCamera* camera = mContext->GetMainCamera();
						if (camera)
							PlanetSystem::GenerateDistanceLUT(PlanetSystem::sNumLevels, PlanetSystem::sRadius, camera->GetPerspectiveVerticalFOV(), std::get<0>(mContext->GetViewportSize()));
					}
				}

				ImGui::EndTable();
			}

			ImGui::EndGroup();

			ImGui::Spacing();            // one line
			ImGui::Spacing();            // another (≈ 10-12 px total)

			// section header
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::TextUnformatted("Terrain Data");
			ImGui::PopFont();

			ImGui::Spacing();

			if (ImGui::BeginTable("TerrainTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.4f);
				ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.6f);

				// -------- Radius row ----------
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Radius");

				ImGui::TableSetColumnIndex(1);

				float padX = ImGui::GetStyle().CellPadding.x;
				float colW = ImGui::GetColumnWidth();             // total width of column 1
				float fullW = colW - padX * 2.0f;                  // leave padding on both sides
				ImGui::SetNextItemWidth(fullW);

				ImGui::InputDouble("##Radius", &PlanetSystem::sRadius, 0.0, 0.0, "%.1f");

				ImGui::EndTable();
			}

			ImGui::PopID();

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();

		ImGui::PopStyleColor(2);
	}
}