#pragma once

#include "PlanetPanel.h"

#include "Toast/Core/Log.h"

#include "Toast/ImGui/ImGuiHelpers.h"

#include "Toast/Renderer/Renderer.h"

#include "Toast/Physics/PhysicsEngine.h"

#include "Toast/Utils/PlatformUtils.h"

#include "../FontAwesome.h"

#include "imgui/imgui.h"

#include <filesystem>

namespace Toast {

	extern const std::filesystem::path gAssetPath;

	void PlanetPanel::SetContext(Scene* context, WindowsWindow* window)
	{
		mContext = context;
		mWindow = window;
	}

	void PlanetPanel::OnImGuiRender(bool* showPanel, std::string& activeDragArea)
	{
		if (!showPanel || !*showPanel)
			return;

		ImGuiIO& io = ImGui::GetIO();

		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		const ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar;

		if (ImGui::Begin("Planet", nullptr, popupFlags))
		{
			const float titleBarHeight = 38.0f;
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
			ImGui::Text(ICON_TOASTER_GLOBE" Planet");
			ImGui::PopFont();

			// Close button
			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + windowSize.x - buttonSize - 6.0f, windowPos.y + 6.0f));
			if (ImGui::Button("X##PlanetClose", ImVec2(buttonSize, buttonSize)))
				*showPanel = false;

			// Push content below title bar
			ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 10.0f, windowPos.y + titleBarHeight + 6.0f));
			
			ImGui::Spacing(); 

			ImGui::Indent(10.0f);

			// section header
			ImGui::PushFont(io.Fonts->Fonts[4]);     
			ImGui::TextUnformatted("Planet Base Data");
			ImGui::PopFont();

			ImGui::PushID("PlanetPopupControls");
			ImGui::BeginGroup();

			static int gridSizes[] = { 33, 65, 129, 257, 513 };
			static int currentGridSize = 129;
			static int currentLOD = 5;

			// Create a 2-column table for label + control layout
			if (ImGui::BeginTable("PlanetTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4);
				ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Translation");

				ImGui::TableSetColumnIndex(1);

				auto  padX = ImGui::GetStyle().CellPadding.x;
				float colW = ImGui::GetColumnWidth();               // full width of this column
				float fullW = colW - padX * 2.0f;

				ImGui::SetNextItemWidth(fullW);

				ImGuiHelpers::ManualDragFloat3("##translation", PlanetSystem::sTranslation, 1.0f, 0.0f, mWindow, activeDragArea);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Rotation");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetNextItemWidth(fullW);

				ImGuiHelpers::ManualDragFloat3("##rotation", PlanetSystem::sRotationEulerAngles, 0.1f, 0.0f, mWindow, activeDragArea);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Grid Size");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetNextItemWidth(fullW);

				std::string gridLabel = std::to_string(PlanetSystem::sTempGridSize);          // keep it alive
				if (ImGui::BeginCombo("##GridSize", gridLabel.c_str()))
				{
					for (int i = 0; i < IM_ARRAYSIZE(gridSizes); ++i)
					{
						bool selected = (currentGridSize == gridSizes[i]);
						if (ImGui::Selectable(std::to_string(gridSizes[i]).c_str(), selected))
							PlanetSystem::sTempGridSize = gridSizes[i];
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
				ImGui::SliderInt("##LOD", &PlanetSystem::sTempNumLevels, 1, 30);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(1);

				const float btnW = 80.0f;                    
				float indent = fullW - btnW; 
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

				if (ImGui::Button("Apply", ImVec2(btnW, 0)))
				{
					if (PlanetSystem::sTempNumLevels != 0 && PlanetSystem::sTempGridSize != 0)
					{
						PlanetSystem::sGridSize = PlanetSystem::sTempGridSize;
						PlanetSystem::sNumLevels = PlanetSystem::sTempNumLevels;

						PlanetSystem::RebuildGrid();
						PlanetSystem::RebuildRingGridIndices();
						PlanetSystem::RebuildLODEdgeGrid();

						PlanetSystem::InitializeLevels();

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
			ImGui::TextUnformatted("Physically Based Rendering");
			ImGui::PopFont();

			ImGui::Spacing();

			if (ImGui::BeginTable("MaterialTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
			{

				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
				ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Albedo Color");

				ImGui::TableSetColumnIndex(1);

				float padX = ImGui::GetStyle().CellPadding.x;
				float colW = ImGui::GetColumnWidth();             // total width of column 1
				float fullW = colW - padX * 2.0f;                  // leave padding on both sides
				ImGui::SetNextItemWidth(fullW);

				ImGui::ColorEdit3("##albedocolor", &PlanetSystem::sAlbedoColor.x);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Roughness");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetNextItemWidth(fullW);

				ImGui::DragFloat("##roughness", &PlanetSystem::sRoughness, 0.01f, 0.0f, 1.0f, "%.2f");

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Metalness");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetNextItemWidth(fullW);

				ImGui::DragFloat("##metallic", &PlanetSystem::sMetalness, 0.01f, 0.0f, 1.0f, "%.2f");

				ImGui::EndTable();
			}

			ImGui::Spacing();            // one line
			ImGui::Spacing();            // another (≈ 10-12 px total)

			// section header
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::TextUnformatted("Terrain Data");
			ImGui::PopFont();

			ImGui::Spacing();

			if (ImGui::BeginTable("TerrainTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
				ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

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

				float temp = PlanetSystem::sRadius;
				if (ImGui::DragFloat("##Radius", &temp, 1.0f, 1.0f, FLT_MAX, "%.0f"))
				{
					PlanetSystem::sRadius = temp;

					SceneCamera* camera = mContext->GetMainCamera();
					if (camera)
						PlanetSystem::GenerateDistanceLUT(PlanetSystem::sNumLevels, PlanetSystem::sRadius, camera->GetPerspectiveVerticalFOV(), std::get<0>(mContext->GetViewportSize()));
				}

				ImGui::TableNextRow();

				// -------- Max Height row ----------

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Max Height");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetNextItemWidth(fullW);

				temp = PlanetSystem::sMaxHeight;
				if (ImGui::DragFloat("##maxheight", &temp, 1.0f, -FLT_MAX, FLT_MAX, "%.0f"))
					PlanetSystem::sMaxHeight = temp;

				ImGui::TableNextRow();

				// -------- Min Height row ----------

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Min Height");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetNextItemWidth(fullW);

				temp = PlanetSystem::sMinHeight;
				if (ImGui::DragFloat("##minheight", &temp, 1.0f, -FLT_MAX, FLT_MAX, "%.0f"))
					PlanetSystem::sMinHeight = temp;

				ImGui::TableNextRow();

				// -------- Height Map Texture row ----------

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Base Height Map Texture");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::GetStyle().CellPadding.x * 2 - 128.0f);
				ImGui::Image(PlanetSystem::sBaseHeightMapTexture->GetID(), { 128.0f, 64.0f });

				std::optional<std::string> filename;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = std::filesystem::path(gAssetPath) / path;
						filename = completePath.string();

						if (filename)
						{
							PlanetSystem::sBaseHeightMapTexture = TextureLibrary::LoadTexture2D(*filename);

							PlanetSystem::sTerrainData = PhysicsEngine::LoadTerrainData(*filename, PlanetSystem::sMaxHeight, PlanetSystem::sMinHeight);
						}
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");

					if (filename)
					{
						PlanetSystem::sBaseHeightMapTexture = TextureLibrary::LoadTexture2D(*filename);

						PlanetSystem::sTerrainData = PhysicsEngine::LoadTerrainData(*filename, PlanetSystem::sMaxHeight, PlanetSystem::sMinHeight);
					}
				}

				ImGui::TableSetColumnIndex(1);

				ImGui::EndTable();
			}

			ImGui::Spacing();            // one line
			ImGui::Spacing();            // another (≈ 10-12 px total)

			// section header
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::TextUnformatted("Star Field");
			ImGui::PopFont();

			ImGui::Spacing();

			if (ImGui::BeginTable("StarMapTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
				ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

				ImGui::TableNextRow();

				// -------- Star Field Texture row ----------

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Star Field Texture");

				ImGui::TableSetColumnIndex(1);

				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::GetStyle().CellPadding.x * 2 - 128.0f);
				ImGui::Image(PlanetSystem::sStarFieldTexture2D->GetID(), { 128.0f, 64.0f });

				std::optional<std::string> filename;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = std::filesystem::path(gAssetPath) / path;
						filename = completePath.string();

						if (filename)
						{
							PlanetSystem::sStarFieldTexture2D = TextureLibrary::LoadTexture2D(*filename);

							PlanetSystem::sStarFieldTextureCube = Renderer::CreateStarFieldTexture(PlanetSystem::sStarFieldTexture2D);
						}
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");

					if (filename)
					{
						PlanetSystem::sStarFieldTexture2D = TextureLibrary::LoadTexture2D(*filename);

						PlanetSystem::sStarFieldTextureCube = Renderer::CreateStarFieldTexture(PlanetSystem::sStarFieldTexture2D);
					}
				}

				ImGui::TableSetColumnIndex(1);

				ImGui::EndTable();
			}

			ImGui::PopID();

			ImGui::End();
		}

		ImGui::PopStyleVar();

		ImGui::PopStyleColor(2);
	}
}