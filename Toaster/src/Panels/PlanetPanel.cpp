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

	void PlanetPanel::SetContext(Scene* sceneContext, WindowsWindow* window)
	{
		mSceneContext = sceneContext;
		mContext = sceneContext->GetPlanet().get();
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
			if (ImGui::BeginChild("PlanetPanelScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
			{
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

					ImGuiHelpers::ManualDragFloat3("##translation", mContext->mTranslation, 1.0f, 0.0f, mWindow, activeDragArea);

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Rotation");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGuiHelpers::ManualDragFloat3("##rotation", mContext->mRotationEulerAngles, 0.1f, 0.0f, mWindow, activeDragArea);

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Grid Size");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					std::string gridLabel = std::to_string(mContext->mTempGridSize);          // keep it alive
					if (ImGui::BeginCombo("##GridSize", gridLabel.c_str()))
					{
						for (int i = 0; i < IM_ARRAYSIZE(gridSizes); ++i)
						{
							bool selected = (currentGridSize == gridSizes[i]);
							if (ImGui::Selectable(std::to_string(gridSizes[i]).c_str(), selected))
								mContext->mTempGridSize = gridSizes[i];
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
					ImGui::SliderInt("##LOD", &mContext->mTempNumLevels, 1, 30);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(1);

					const float btnW = 80.0f;                    
					float indent = fullW - btnW; 
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

					if (ImGui::Button("Apply", ImVec2(btnW, 0)))
					{
						if (mContext->mTempNumLevels != 0 && mContext->mTempGridSize != 0)
						{
							mContext->mGridSize = mContext->mTempGridSize;
							mContext->mNumLevels = mContext->mTempNumLevels;

							mContext->RebuildGrid();
							mContext->RebuildRingGridIndices();
							mContext->RebuildLODEdgeGrid();

							mContext->InitializeLevels();

							SceneCamera* camera = mSceneContext->GetMainCamera();
							if (camera)
								mContext->GenerateDistanceLUT(mContext->mNumLevels, mContext->mRadius, camera->GetPerspectiveVerticalFOV(), std::get<0>(mSceneContext->GetViewportSize()));
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

					ImGui::ColorEdit3("##albedocolor", &mContext->mAlbedoColor.x);

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Roughness");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##roughness", &mContext->mRoughness, 0.01f, 0.0f, 1.0f, "%.2f");

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Metalness");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##metallic", &mContext->mMetalness, 0.01f, 0.0f, 1.0f, "%.2f");

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

					float temp = mContext->mRadius;
					if (ImGui::DragFloat("##Radius", &temp, 1.0f, 1.0f, FLT_MAX, "%.0f"))
					{
						mContext->mRadius = temp;

						SceneCamera* camera = mSceneContext->GetMainCamera();
						if (camera)
							mContext->GenerateDistanceLUT(mContext->mNumLevels, mContext->mRadius, camera->GetPerspectiveVerticalFOV(), std::get<0>(mSceneContext->GetViewportSize()));
					}

					ImGui::TableNextRow();

					// -------- Max Height row ----------

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Max Height");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					temp = mContext->mMaxHeight;
					if (ImGui::DragFloat("##maxheight", &temp, 1.0f, -FLT_MAX, FLT_MAX, "%.0f"))
						mContext->mMaxHeight = temp;

					ImGui::TableNextRow();

					// -------- Min Height row ----------

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Min Height");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					temp = mContext->mMinHeight;
					if (ImGui::DragFloat("##minheight", &temp, 1.0f, -FLT_MAX, FLT_MAX, "%.0f"))
						mContext->mMinHeight = temp;

					ImGui::TableNextRow();

					// -------- Height Map Texture row ----------

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Base Height Map Texture");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::GetStyle().CellPadding.x * 2 - 128.0f);
					ImGui::Image(mContext->mBaseHeightMapTexture->GetID(), { 128.0f, 64.0f });

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
								mContext->mBaseHeightMapTexture = TextureLibrary::LoadTexture2D(*filename, false);

								mContext->mTerrainData = PhysicsEngine::LoadTerrainData(*filename, mContext->mMaxHeight, mContext->mMinHeight);
							}
						}

						ImGui::EndDragDropTarget();
					}

					if (ImGui::IsItemClicked())
					{
						filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");

						if (filename)
						{
							mContext->mBaseHeightMapTexture = TextureLibrary::LoadTexture2D(*filename, false);

							mContext->mTerrainData = PhysicsEngine::LoadTerrainData(*filename, mContext->mMaxHeight, mContext->mMinHeight);
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
					ImGui::Image(mContext->mStarFieldTexture2D->GetID(), { 128.0f, 64.0f });

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
								mContext->mStarFieldTexture2D = TextureLibrary::LoadTexture2D(*filename);

								mContext->mStarFieldTextureCube = Renderer::CreateStarFieldTexture(mContext->mStarFieldTexture2D);
							}
						}

						ImGui::EndDragDropTarget();
					}

					if (ImGui::IsItemClicked())
					{
						filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");

						if (filename)
						{
							mContext->mStarFieldTexture2D = TextureLibrary::LoadTexture2D(*filename);

							mContext->mStarFieldTextureCube = Renderer::CreateStarFieldTexture(mContext->mStarFieldTexture2D);
						}
					}

					ImGui::TableSetColumnIndex(1);

					ImGui::EndTable();
				}

				ImGui::PushFont(io.Fonts->Fonts[4]);
				ImGui::TextUnformatted("Atmospheric Scattering");
				ImGui::PopFont();

				ImGui::Spacing();

				if (ImGui::BeginTable("AtmosphericScatteringTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
				{
					ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
					ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

					// -------- Atmosphere Height Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Activate Atmosphere");
					ImGui::TableSetColumnIndex(1);
					ImGui::Checkbox("##activateatmosphere", &mContext->mAtmosphereActivated);
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Atmosphere Height");

					ImGui::TableSetColumnIndex(1);

					float padX = ImGui::GetStyle().CellPadding.x;
					float colW = ImGui::GetColumnWidth();             // total width of column 1
					float fullW = colW - padX * 2.0f;                  // leave padding on both sides
					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##AtmosphereHeight", &mContext->mAtmosphere.AtmosphereHeight, 1.0f, 0.0f, FLT_MAX, "%.0f");

					// -------- Rayleigh Scale Height Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Rayleigh Scale Height");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##RayleighScaleHeight", &mContext->mAtmosphere.RayleighScaleHeight, 1.0f, 0.0f, FLT_MAX, "%.0f");

					// -------- Rayleigh Scattering Red Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Rayleigh Scattering Red");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##RayleighScatteringRed", &mContext->mAtmosphere.RayleighScattering.x, 0.00000001f, 0.0f, FLT_MAX, "%.9f");

					// -------- Rayleigh Scattering Green Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Rayleigh Scattering Green");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##RayleighScatteringGreen", &mContext->mAtmosphere.RayleighScattering.y, 0.00000001f, 0.0f, FLT_MAX, "%.9f");
				
					// -------- Rayleigh Scattering Blue Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Rayleigh Scattering Blue");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##RayleighScatteringBlue", &mContext->mAtmosphere.RayleighScattering.z, 0.00000001f, 0.0f, FLT_MAX, "%.9f");

					// -------- Mie Scale Height Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Scale Height");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieScaleHeight", &mContext->mAtmosphere.MieScaleHeight, 1.0f, 0.0f, FLT_MAX, "%.0f");

					// -------- Mie Scattering Red Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Scattering Red");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieScatteringRed", &mContext->mAtmosphere.MieScattering.x, 0.000001f, 0.0f, FLT_MAX, "%.7f");

					// -------- Mie Scattering Green Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Scattering Green");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieScatteringGreen", &mContext->mAtmosphere.MieScattering.y, 0.000001f, 0.0f, FLT_MAX, "%.7f");

					// -------- Mie Scattering Blue Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Scattering Blue");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieScatteringBlue", &mContext->mAtmosphere.MieScattering.z, 0.000001f, 0.0f, FLT_MAX, "%.7f");

					// -------- Mie Absorption Red Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Absorption Red");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieAbsorptionRed", &mContext->mAtmosphere.MieAbsorption.x, 0.000001f, 0.0f, FLT_MAX, "%.7f");

					// -------- Mie Absorption Green Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Absorption Green");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieAbsorptionGreen", &mContext->mAtmosphere.MieAbsorption.y, 0.000001f, 0.0f, FLT_MAX, "%.7f");

					// -------- Mie Absorption Blue Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Absorption Blue");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieAbsorptionBlue", &mContext->mAtmosphere.MieAbsorption.z, 0.000001f, 0.0f, FLT_MAX, "%.7f");

					// -------- Mie Anisotropy Red Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Anisotropy Red");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieAnisotropyRed", &mContext->mAtmosphere.MieAnisotropy.x, 0.01f, 0.0f, FLT_MAX, "%.2f");

					// -------- Mie Anisotropy Green Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Anisotropy Green");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieAnisotropyGreen", &mContext->mAtmosphere.MieAnisotropy.y, 0.01f, 0.0f, FLT_MAX, "%.2f");

					// -------- Mie Anisotropy Blue Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Mie Anisotropy Blue");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##MieAnisotropyBlue", &mContext->mAtmosphere.MieAnisotropy.z, 0.01f, 0.0f, FLT_MAX, "%.2f");

					// -------- Ozone Strength Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Ozone Strength");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##OzoneStrength", &mContext->mAtmosphere.OzoneStrength, 1.0f, 0.0f, FLT_MAX, "%.0f");

					// -------- Ground Albedo Row ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Ground Albedo");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::ColorEdit3("##GroundAlbedo", &mContext->mAtmosphere.GroundAlbedo.x);

					// -------- Sunset Tint Color ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Sunset Tint");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGuiHelpers::ManualDragFloat3("##SunsetTint", mContext->mAtmosphere.SunsetTint, 0.01f, 0.0f, mWindow, activeDragArea, "%.2f", true);

					// -------- MS Gain ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Multi Scattering Gain");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##multiscatteringgain", &mContext->mAtmosphere.MSGain, 0.1f, 0.0f, FLT_MAX, "%.1f");

					// -------- SS Gain ----------
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Single Scattering Gain");

					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(fullW);

					ImGui::DragFloat("##singelscatteringgain", &mContext->mAtmosphere.SGain, 0.1f, 0.0f, FLT_MAX, "%.1f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(1);

					const float btnW = 80.0f;
					float indent = fullW - btnW;
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

					if(ImGui::Button("Apply", ImVec2(btnW, 0))) 
					{
						Renderer::GenerateTransmittanceLUT(mContext);
						Renderer::GenerateMultiScatteringLUT(mContext);
					}

					ImGui::EndTable();
				}

				ImGui::Spacing();
				ImGui::Spacing();

				ImGui::PopID();

				ImGui::Spacing();
			}
			ImGui::EndChild();
			ImGui::End();
		}

		ImGui::PopStyleVar();

		ImGui::PopStyleColor(2);
	}
}