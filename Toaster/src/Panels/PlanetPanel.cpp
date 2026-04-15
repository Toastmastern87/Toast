#pragma once

#include "PlanetPanel.h"

#include "Toast/Core/Log.h"

#include "Toast/ImGui/ImGuiHelpers.h"

#include "Toast/Renderer/Renderer.h"

#include "Toast/Physics/PhysicsEngine.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Utils/PlatformUtils.h"

#include "../FontAwesome.h"

#include "imgui/imgui.h"

#include <filesystem>

namespace Toast {

	static void CopyToNameBuf(char* dst, size_t dstSize, const std::string& src)
	{
		if (!dst || dstSize == 0) return;
		std::snprintf(dst, dstSize, "%s", src.c_str());
		dst[dstSize - 1] = '\0';
	}

	static void CopyFromNameBuf(std::string& dst, const char* src)
	{
		dst = (src && src[0]) ? std::string(src) : std::string("New Terrain Detail");
	}

	void PlanetPanel::SetContext(Scene* sceneContext, WindowsWindow* window)
	{
		mSceneContext = sceneContext;
		mContext = sceneContext->GetPlanet().get();
		mWindow = window;
	}

	void PlanetPanel::SetProjectPath(const std::filesystem::path& projectPath)
	{
		mAssetRoot = projectPath / "Assets";
	}

	void PlanetPanel::DrawTerrainObjectsListUI()
	{
		// --- Visual sizing: show up to 3 items without scrolling ---
		const float visibleItems = 3.0f;
		const float rowH = ImGui::GetFrameHeight();
		const float rowGap = 1.0f;
		const float innerPadY = 8.0f * 2.0f;

		auto  padX = ImGui::GetStyle().CellPadding.x;
		float colW = ImGui::GetColumnWidth();               // full width of this column
		float fullW = colW - padX * 2.0f;

		float minBoxH = innerPadY + visibleItems * rowH + (visibleItems - 1.0f) * rowGap;
		float boxH = minBoxH;

		ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoMove;

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Terrain Objects");

		ImGui::TableSetColumnIndex(1);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 0.0f));
		ImGui::BeginChild("##PlanetTerrainObjectsBox", ImVec2(fullW, boxH), true, childFlags);
		ImGui::Dummy(ImVec2(0.0f, 0.5f));

		int deleteIndex = -1;
		// Render each object as a “box” row (clickable)
		for (int i = 0; i < (int)mContext->mTerrainObjects.size(); ++i)
		{
			TerrainObject& o = mContext->mTerrainObjects[i];
			ImGui::PushID(i);

			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

			float w = ImGui::GetContentRegionAvail().x;
			bool clicked = ImGui::Button(o.Name.c_str(), ImVec2(w, 0.0f));

			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(1);

			if (ImGui::BeginPopupContextItem("##DetailContext", ImGuiPopupFlags_MouseButtonRight))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 6.0f));

				if (ImGui::MenuItem("Delete"))
					deleteIndex = i;

				ImGui::PopStyleVar(2);
				ImGui::EndPopup();
			}

			ImGui::Dummy(ImVec2(0.0f, 0.5f));

			if (clicked)
			{
				mEditingTerrainObj = true;
				mEditingTerrainObjIndex = i;

				mTerrainObjDraft = o; // optional; if you want direct edit, you can skip draft for edit mode
				CopyToNameBuf(mTerrainObjNameBuf, sizeof(mTerrainObjNameBuf), o.Name);

				// Mesh path buffer
				const std::string path = (o.MeshObject && !o.MeshObject->GetFilePath().empty())
					? o.MeshObject->GetFilePath()
					: std::string("Empty");
				memset(mTerrainObjMeshPathBuf, 0, sizeof(mTerrainObjMeshPathBuf));
				strncpy_s(mTerrainObjMeshPathBuf, path.c_str(), sizeof(mTerrainObjMeshPathBuf) - 1);

				mRequestOpenTerrainObjPopup = true;
			}

			ImGui::PopID();
		}

		if (deleteIndex != -1)
		{
			// If you are editing this one (or indices after it), fix state.
			if (mEditingTerrainObj)
			{
				if (mEditingTerrainObjIndex == deleteIndex)
				{
					mEditingTerrainObj = false;
					mEditingTerrainObjIndex = -1;
				}
				else if (mEditingTerrainObjIndex > deleteIndex)
				{
					// Vector elements shift left
					mEditingTerrainObjIndex--;
				}
			}

			mContext->mTerrainObjects.erase(mContext->mTerrainObjects.begin() + deleteIndex);
		}

		ImGui::EndChild();
		ImGui::PopStyleVar();

		// --- Add button aligned bottom-right of the column ---
		{
			const bool disableAdd = (mContext->mTerrainObjects.size() >= 8); // pick your cap

			const float btnSize = ImGui::GetFrameHeight();
			float cursorX = ImGui::GetCursorPosX();
			float availX = ImGui::GetContentRegionAvail().x;

			ImGui::SetCursorPosX(cursorX + (availX - btnSize - 7.0f));

			ImGui::BeginDisabled(disableAdd);
			if (ImGui::Button("+##AddTerrainObject", ImVec2(btnSize, btnSize)))
			{
				mEditingTerrainObj = false;
				mEditingTerrainObjIndex = -1;

				mTerrainObjDraft = TerrainObject{};
				mTerrainObjDraft.Name = "New Terrain Object";

				// seed like you do for height details
				static std::mt19937 rng{ std::random_device{}() };
				mTerrainObjDraft.Seed = (uint32_t)rng(); // add a Seed member if you want it shown like height details

				// sensible defaults
				mTerrainObjDraft.LODActivation = 1;
				mTerrainObjDraft.DensityPerKm2 = 2000.0f;
				mTerrainObjDraft.MaxPerPatch = 16;
				mTerrainObjDraft.MaxTotal = 200000;
				mTerrainObjDraft.MinScale = 0.1f;
				mTerrainObjDraft.MaxScale = 0.3f;

				CopyToNameBuf(mTerrainObjNameBuf, sizeof(mTerrainObjNameBuf), mTerrainObjDraft.Name);

				memset(mTerrainObjMeshPathBuf, 0, sizeof(mTerrainObjMeshPathBuf));
				strncpy_s(mTerrainObjMeshPathBuf, "Empty", sizeof(mTerrainObjMeshPathBuf) - 1);

				mRequestOpenTerrainObjPopup = true;
			}
			ImGui::EndDisabled();
		}

		if (mRequestOpenTerrainObjPopup)
		{
			ImGui::OpenPopup("##TerrainObjectPopup");
			mRequestOpenTerrainObjPopup = false;
		}
	}

	void PlanetPanel::DrawTerrainObjectMeshRow(TerrainObject& target)
	{
		ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit;
		ImVec2 avail = ImGui::GetContentRegionAvail();

		if (ImGui::BeginTable("##TerrainObjMeshTable", 3, flags))
		{
			ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, avail.x * 0.6156f);
			ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Mesh");

			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-1);

			const std::string path =
				(target.MeshObject && !target.MeshObject->GetFilePath().empty())
				? target.MeshObject->GetFilePath()
				: std::string("Empty");

			// Keep buffer synced
			memset(mTerrainObjMeshPathBuf, 0, sizeof(mTerrainObjMeshPathBuf));
			strncpy_s(mTerrainObjMeshPathBuf, path.c_str(), sizeof(mTerrainObjMeshPathBuf) - 1);

			ImGui::InputText("##terrainobj_meshfilepath", mTerrainObjMeshPathBuf, sizeof(mTerrainObjMeshPathBuf),
				ImGuiInputTextFlags_ReadOnly);

			ImGui::PopItemWidth();

			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button("...##open_terrainobj_mesh"))
			{
				std::optional<std::string> filepath =
					FileDialogs::OpenFile("*.gltf", "..\\Toaster\\assets\\meshes\\");

				if (filepath)
				{
					target.MeshObject = CreateRef<Mesh>(*filepath);

					// If you're adding a new object, auto-name it from file
					if (!mEditingTerrainObj && target.Name == "New Terrain Object")
					{
						std::string newName = *filepath;
						std::size_t found = newName.find_last_of("/\\");
						newName = newName.substr(found + 1);
						found = newName.find_last_of('.');
						if (found != std::string::npos)
							newName = newName.substr(0, found);

						target.Name = newName;
						CopyToNameBuf(mTerrainObjNameBuf, sizeof(mTerrainObjNameBuf), target.Name);
					}
				}
			}

			ImGui::EndTable();
		}
	}

	void PlanetPanel::RequestTextureImport(const std::filesystem::path& path,  bool defaultSRGB,	std::function<void(AssetHandle)> onComplete)
	{
		// Check if this file is already registered — no need for the popup
		auto assetDir = AssetManager::GetAssetDirectory();
		auto relativePath = std::filesystem::relative(path, assetDir);
		AssetHandle existing = AssetManager::GetHandleFromPath(relativePath);

		if (existing != AssetHandle(0))
		{
			if (onComplete)
				onComplete(existing);
			return;
		}

		mPendingImportPath = path;
		mPendingImportSRGB = defaultSRGB;
		mOnImportComplete = onComplete;
		mPendingImportOpen = true;
	}

	void PlanetPanel::DrawImportTexturePopup()
	{
		if (!mPendingImportOpen)
			return;

		ImGui::OpenPopup("Import Texture Settings");

		if (ImGui::BeginPopupModal("Import Texture Settings", &mPendingImportOpen,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Importing: %s", mPendingImportPath.filename().string().c_str());
			ImGui::Checkbox("sRGB (color data)", &mPendingImportSRGB);

			if (ImGui::Button("Import"))
			{
				AssetHandle handle = AssetManager::ImportExternalAsset(mPendingImportPath, "Textures");

				AssetEntry* entry = AssetManager::GetEntry(handle);
				if (entry)
					entry->Texture2DSettings.ForceSRGB = mPendingImportSRGB;

				if (mOnImportComplete)
					mOnImportComplete(handle);

				mPendingImportOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				mPendingImportOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void PlanetPanel::DrawTerrainObjectPopup()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

		if (ImGui::BeginPopupModal("##TerrainObjectPopup", nullptr,	ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
		{
			TerrainObject* live = nullptr;

			if (mEditingTerrainObj &&
				mEditingTerrainObjIndex >= 0 &&
				mEditingTerrainObjIndex < (int)mContext->mTerrainObjects.size())
			{
				live = &mContext->mTerrainObjects[mEditingTerrainObjIndex];
			}

			// Edit live when editing; otherwise edit draft
			TerrainObject& target = (live != nullptr) ? *live : mTerrainObjDraft;

			const char* popupHeader = mEditingTerrainObj ? "Edit Terrain Object" : "Add Terrain Object";
			ImGui::TextUnformatted(popupHeader);
			ImGui::Separator();

			// Name
			ImGui::Text("Name");
			ImGui::SetNextItemWidth(360.0f);
			if (ImGui::InputText("##TerrainObjName", mTerrainObjNameBuf, sizeof(mTerrainObjNameBuf)))
				CopyFromNameBuf(target.Name, mTerrainObjNameBuf);

			// LODActivation
			ImGui::Text("LOD Activation");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragInt("##TerrainObjLOD", &target.LODActivation, 1.0f, 0, 25);

			// Seed (read-only)
			ImGui::Text("Seed");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::BeginDisabled();
			uint32_t seed = target.Seed;
			ImGui::InputScalar("##TerrainObjSeed", ImGuiDataType_U32, &seed);
			ImGui::EndDisabled();

			ImGui::Separator();

			// --- Mesh row (table) ---
			DrawTerrainObjectMeshRow(target); // implement below

			ImGui::Separator();

			// Density / caps
			ImGui::Text("Density (per km^2)");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragFloat("##TerrainObjDensity", &target.DensityPerKm2, 10.0f, 0.0f, 1e8f, "%.0f");

			ImGui::Text("Max per patch");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragInt("##TerrainObjMaxPerPatch", &target.MaxPerPatch, 1.0f, 0, 4096);

			ImGui::Text("Max total");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragInt("##TerrainObjMaxTotal", &target.MaxTotal, 256.0f, 0, 5000000);

			ImGui::Separator();

			// Scale
			ImGui::Text("Min scale");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragFloat("##TerrainObjMinScale", &target.MinScale, 0.01f, 0.0f, 100.0f, "%.2f");

			ImGui::Text("Max scale");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragFloat("##TerrainObjMaxScale", &target.MaxScale, 0.01f, 0.0f, 100.0f, "%.2f");

			if (target.MaxScale < target.MinScale)
				target.MaxScale = target.MinScale;

			ImGui::Separator();

			const float btnW = 120.0f;

			if (ImGui::Button("Close", ImVec2(btnW, 0.0f)))
			{
				mEditingTerrainObj = false;
				mEditingTerrainObjIndex = -1;
				ImGui::CloseCurrentPopup();
			}

			if (!mEditingTerrainObj)
			{
				ImGui::SameLine();

				// Optional: do not allow Add unless mesh is selected
				const bool canAdd = (mTerrainObjDraft.MeshObject != nullptr);
				ImGui::BeginDisabled(!canAdd);

				if (ImGui::Button("Add", ImVec2(btnW, 0.0f)))
				{
					mContext->mTerrainObjects.push_back(mTerrainObjDraft);
					mEditingTerrainObj = false;
					mEditingTerrainObjIndex = -1;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndDisabled();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar(3);
	}

	void PlanetPanel::DrawTerrainMaterialsListUI()
	{
		// --- Visual sizing: show up to 3 items without scrolling ---
		const float visibleItems = 3.0f;
		const float rowH = ImGui::GetFrameHeight();
		const float rowGap = 1.0f;
		const float innerPadY = 8.0f * 2.0f;
		auto  padX = ImGui::GetStyle().CellPadding.x;
		float colW = ImGui::GetColumnWidth();
		float fullW = colW - padX * 2.0f;
		float minBoxH = innerPadY + visibleItems * rowH + (visibleItems - 1.0f) * rowGap;
		float boxH = minBoxH;

		ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar
			| ImGuiWindowFlags_NoMove;

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Terrain Materials");

		ImGui::TableSetColumnIndex(1);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 0.0f));
		ImGui::BeginChild("##PlanetTerrainMaterialsBox", ImVec2(fullW, boxH), true, childFlags);
		ImGui::Dummy(ImVec2(0.0f, 0.5f));

		int deleteIndex = -1;

		for (int i = 0; i < (int)mContext->mMaterials.size(); ++i)
		{
			PlanetMaterial& mat = mContext->mMaterials[i];
			ImGui::PushID(i);

			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
				ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

			float w = ImGui::GetContentRegionAvail().x;
			bool clicked = ImGui::Button(mat.Name.c_str(), ImVec2(w, 0.0f));

			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(1);

			// Right-click context menu for delete
			if (ImGui::BeginPopupContextItem("##MaterialContext",
				ImGuiPopupFlags_MouseButtonRight))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 6.0f));
				if (ImGui::MenuItem("Delete"))
					deleteIndex = i;
				ImGui::PopStyleVar(2);
				ImGui::EndPopup();
			}

			ImGui::Dummy(ImVec2(0.0f, 0.5f));

			if (clicked)
			{
				mEditingMaterial = true;
				mEditingMaterialIndex = i;
				mEditingNoiseLayer = false;
				mEditingNoiseLayerIndex = -1;
				CopyToNameBuf(mMaterialNameBuf, sizeof(mMaterialNameBuf), mat.Name);
				mRequestOpenMaterialPopup = true;
			}

			ImGui::PopID();
		}

		// Handle deletion
		if (deleteIndex != -1)
		{
			if (mEditingMaterial)
			{
				if (mEditingMaterialIndex == deleteIndex)
				{
					mEditingMaterial = false;
					mEditingMaterialIndex = -1;
				}
				else if (mEditingMaterialIndex > deleteIndex)
				{
					mEditingMaterialIndex--;
				}
			}
			mContext->mMaterials.erase(	mContext->mMaterials.begin() + deleteIndex);
			mContext->mMaterialsIsDirty = true;
		}

		ImGui::EndChild();
		ImGui::PopStyleVar();

		// --- Add button aligned bottom-right ---
		{
			const bool disableAdd = (mContext->mMaterials.size() >= 8);
			const float btnSize = ImGui::GetFrameHeight();
			float cursorX = ImGui::GetCursorPosX();
			float availX = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(cursorX + (availX - btnSize - 7.0f));

			ImGui::BeginDisabled(disableAdd);
			if (ImGui::Button("+##AddTerrainMaterial", ImVec2(btnSize, btnSize)))
			{
				mEditingMaterial = false;
				mEditingMaterialIndex = -1;
				mEditingNoiseLayer = false;
				mEditingNoiseLayerIndex = -1;
				mMaterialDraft = PlanetMaterial{};
				mMaterialDraft.Name = "New Material";
				CopyToNameBuf(mMaterialNameBuf, sizeof(mMaterialNameBuf),
					mMaterialDraft.Name);
				mRequestOpenMaterialPopup = true;
			}
			ImGui::EndDisabled();
		}

		if (mRequestOpenMaterialPopup)
		{
			ImGui::OpenPopup("##TerrainMaterialPopup");
			mRequestOpenMaterialPopup = false;
		}
	}

	void PlanetPanel::DrawPBRTextureSlot(const char* label, const char* id,	AssetHandle& handle)
	{
		ImGui::PushID(id);

		ImGui::Text("%s", label);

		// Resolve display texture: assigned texture or checkerboard fallback
		Texture2D* displayTexture = dynamic_cast<Texture2D*>(
			TextureLibrary::Get("assets/textures/Checkerboard.png"));
		if (handle != 0)
		{
			auto tex = AssetManager::GetAsset<Texture2D>(AssetHandle(handle));
			if (tex)
				displayTexture = tex.get();
		}

		ImGui::Image(displayTexture->GetID(), { 64.0f, 64.0f });

		// Drag-drop from ContentBrowser
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const wchar_t* path = (const wchar_t*)payload->Data;
				auto completePath = mAssetRoot / path;
				std::string filename = completePath.string();

				RequestTextureImport(filename, false, [&handle, this](AssetHandle h)
					{
						handle = (uint64_t)h;
						if (mEditingMaterial)
							mContext->mMaterialsIsDirty = true;
					});
			}
			ImGui::EndDragDropTarget();
		}

		// Click to browse
		if (ImGui::IsItemClicked())
		{
			auto texturePath = mAssetRoot / "Textures";
			auto filename = FileDialogs::OpenFile("", texturePath.string().c_str());
			if (filename)
			{
				RequestTextureImport(*filename, false, [&handle, this](AssetHandle h)
					{
						handle = (uint64_t)h;
						if (mEditingMaterial)
							mContext->mMaterialsIsDirty = true;
					});
			}
		}

		ImGui::PopID();
	}

	void PlanetPanel::DrawTerrainMaterialPopup()
	{
		ImGuiIO& io = ImGui::GetIO();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));

		if (ImGui::BeginPopupModal("##TerrainMaterialPopup", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
		{
			// Resolve target
			PlanetMaterial* liveMat = nullptr;
			if (mEditingMaterial && mEditingMaterialIndex >= 0
				&& mEditingMaterialIndex < (int)mContext->mMaterials.size())
				liveMat = &mContext->mMaterials[mEditingMaterialIndex];

			PlanetMaterial& target = (liveMat != nullptr) ? *liveMat : mMaterialDraft;
			const char* header = mEditingMaterial
				? "Edit Terrain Material" : "Add Terrain Material";

			ImGui::PushFont(io.Fonts->Fonts[3]);
			ImGui::TextUnformatted(header);
			ImGui::PopFont();
			ImGui::Separator();

			// ── Name ──
			ImGui::Text("Name");
			ImGui::SetNextItemWidth(360.0f);
			if (ImGui::InputText("##MatName", mMaterialNameBuf, sizeof(mMaterialNameBuf)))
				CopyFromNameBuf(target.Name, mMaterialNameBuf);

			// ════════════════════════════════════════════════════════
			//  Selection
			// ════════════════════════════════════════════════════════
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::Text("Selection");
			ImGui::PopFont();

			ImGui::Text("Slope Min");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##SlopeMin", &target.GPU.SlopeMin, 0.01f, 0.0f, 1.0f) && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Text("Slope Max");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##SlopeMax", &target.GPU.SlopeMax, 0.01f, 0.0f, 1.0f) && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Text("Color Avg Min");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##ColorAvgMin", &target.GPU.ColorAvgMin, 0.01f, 0.0f, 1.0f) && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Text("Color Avg Max");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##ColorAvgMax", &target.GPU.ColorAvgMax,
				0.01f, 0.0f, 1.0f) && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			{
				bool useAlbedo = target.GPU.UseAlbedo > 0.5f;
				if (ImGui::Checkbox("Use Albedo", &useAlbedo))
				{
					target.GPU.UseAlbedo = useAlbedo ? 1.0f : 0.0f;
					if (mEditingMaterial) 
						mContext->mMaterialsIsDirty = true;
				}
			}

			ImGui::Text("Blend Sharpness");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##BlendSharpness", &target.GPU.BlendSharpness,
				0.1f, 1.0f, 50.0f) && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// ════════════════════════════════════════════════════════
			//  Noise Layers (nested list inside popup)
			// ════════════════════════════════════════════════════════
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::Text("Noise Layers");
			ImGui::PopFont();

			// Mini scrollable list of noise layers, same style as the material list
			{
				const float visibleItems = 3.0f;
				const float rowH = ImGui::GetFrameHeight();
				const float rowGap = 1.0f;
				const float innerPadY = 8.0f * 2.0f;
				float boxW = 360.0f;
				float boxH = innerPadY + visibleItems * rowH
					+ (visibleItems - 1.0f) * rowGap;

				ImGuiWindowFlags childFlags =
					ImGuiWindowFlags_AlwaysVerticalScrollbar
					| ImGuiWindowFlags_NoMove;

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 0.0f));
				ImGui::BeginChild("##NoiseLayersBox", ImVec2(boxW, boxH),
					true, childFlags);
				ImGui::Dummy(ImVec2(0.0f, 0.5f));

				int deleteLayerIndex = -1;

				for (int n = 0; n < (int)target.NoiseLayers.size(); ++n)
				{
					NoiseLayer& layer = target.NoiseLayers[n];
					ImGui::PushID(n);

					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
						ImVec2(10.0f, 4.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
						ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

					float w = ImGui::GetContentRegionAvail().x;
					bool clicked = ImGui::Button(layer.Name.c_str(),
						ImVec2(w, 0.0f));

					ImGui::PopStyleVar(3);
					ImGui::PopStyleColor(1);

					// Right-click to delete
					if (ImGui::BeginPopupContextItem("##NoiseLayerContext",
						ImGuiPopupFlags_MouseButtonRight))
					{
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
							ImVec2(12.0f, 8.0f));
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
							ImVec2(10.0f, 6.0f));
						if (ImGui::MenuItem("Delete"))
							deleteLayerIndex = n;
						ImGui::PopStyleVar(2);
						ImGui::EndPopup();
					}

					ImGui::Dummy(ImVec2(0.0f, 0.5f));

					if (clicked)
					{
						mEditingNoiseLayer = true;
						mEditingNoiseLayerIndex = n;
						CopyToNameBuf(mNoiseLayerNameBuf,
							sizeof(mNoiseLayerNameBuf), layer.Name);
						mRequestOpenNoiseLayerPopup = true;
					}

					ImGui::PopID();
				}

				// Handle noise layer deletion
				if (deleteLayerIndex != -1)
				{
					if (mEditingNoiseLayer)
					{
						if (mEditingNoiseLayerIndex == deleteLayerIndex)
						{
							mEditingNoiseLayer = false;
							mEditingNoiseLayerIndex = -1;
						}
						else if (mEditingNoiseLayerIndex > deleteLayerIndex)
						{
							mEditingNoiseLayerIndex--;
						}
					}
					target.RemoveNoiseLayer(deleteLayerIndex);
					if (mEditingMaterial) mContext->mMaterialsIsDirty = true;
				}

				ImGui::EndChild();
				ImGui::PopStyleVar();

				// Add noise layer button (bottom-right)
				{
					const bool disableAdd = (target.NoiseLayers.size() >= 8);
					const float btnSize = ImGui::GetFrameHeight();
					float cursorX = ImGui::GetCursorPosX();
					ImGui::SetCursorPosX(cursorX + (boxW - btnSize - 7.0f));

					ImGui::BeginDisabled(disableAdd);
					if (ImGui::Button("+##AddNoiseLayer", ImVec2(btnSize, btnSize)))
					{
						mEditingNoiseLayer = false;
						mEditingNoiseLayerIndex = -1;
						mNoiseLayerDraft = NoiseLayer{};
						mNoiseLayerDraft.Name = "New Noise Layer";
						CopyToNameBuf(mNoiseLayerNameBuf,
							sizeof(mNoiseLayerNameBuf),
							mNoiseLayerDraft.Name);
						mRequestOpenNoiseLayerPopup = true;
					}
					ImGui::EndDisabled();
				}
			}

			// Open noise layer popup if requested
			if (mRequestOpenNoiseLayerPopup)
			{
				ImGui::OpenPopup("##NoiseLayerPopup");
				mRequestOpenNoiseLayerPopup = false;
			}

			// Draw the nested noise layer popup (renders on top of this one)
			DrawNoiseLayerPopup(target);

			// ════════════════════════════════════════════════════════
			//  PBR Textures
			// ════════════════════════════════════════════════════════
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::Text("PBR Textures");
			ImGui::PopFont();

			ImGui::Text("LOD Activation");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragInt("##PBRLOD", &target.PBR.LODActivation, 1.0f, 0, 25) && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Text("Blend Range");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragInt("##PBRBlendRange", &target.PBR.BlendRange, 1.0f, 1, 10) && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Text("Tiling Scale");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##TilingScale", &target.PBR.TilingScale, 0.1f, 0.1f, 100.0f, "%.1f") && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Text("Displacement Strength");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##DispStrength", &target.PBR.DisplacementStrength,
				0.01f, 0.0f, 10.0f, "%.2f") && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			DrawPBRTextureSlot("Albedo", "##PBRAlbedo", target.PBR.AlbedoHandle);
			DrawPBRTextureSlot("Normal", "##PBRNormal", target.PBR.NormalHandle);
			DrawPBRTextureSlot("Roughness", "##PBRRough", target.PBR.RoughnessHandle);
			DrawPBRTextureSlot("AO", "##PBRAO", target.PBR.AOHandle);
			DrawPBRTextureSlot("Displacement", "##PBRDisp", target.PBR.DisplacementHandle);

			// ════════════════════════════════════════════════════════
			//  Close / Add buttons
			// ════════════════════════════════════════════════════════
			ImGui::Spacing();
			ImGui::Separator();
			const float btnW = 120.0f;

			if (ImGui::Button("Close", ImVec2(btnW, 0.0f)))
			{
				mEditingMaterial = false;
				mEditingMaterialIndex = -1;
				mEditingNoiseLayer = false;
				mEditingNoiseLayerIndex = -1;
				ImGui::CloseCurrentPopup();
			}

			if (!mEditingMaterial)
			{
				ImGui::SameLine();
				if (ImGui::Button("Add", ImVec2(btnW, 0.0f)))
				{
					mContext->mMaterials.push_back(mMaterialDraft);
					mEditingMaterial = false;
					mEditingMaterialIndex = -1;
					mEditingNoiseLayer = false;
					mEditingNoiseLayerIndex = -1;
					mContext->mMaterialsIsDirty = true;
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar(3);
	}

	void PlanetPanel::DrawNoiseLayerPopup(PlanetMaterial& parentMaterial)
	{
		ImGuiIO& io = ImGui::GetIO();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));

		if (ImGui::BeginPopupModal("##NoiseLayerPopup", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
		{
			// Resolve target
			NoiseLayer* liveLayer = nullptr;
			if (mEditingNoiseLayer && mEditingNoiseLayerIndex >= 0
				&& mEditingNoiseLayerIndex < (int)parentMaterial.NoiseLayers.size())
				liveLayer = &parentMaterial.NoiseLayers[mEditingNoiseLayerIndex];

			NoiseLayer& target = (liveLayer != nullptr) ? *liveLayer : mNoiseLayerDraft;
			const char* header = mEditingNoiseLayer
				? "Edit Noise Layer" : "Add Noise Layer";

			ImGui::PushFont(io.Fonts->Fonts[3]);
			ImGui::TextUnformatted(header);
			ImGui::PopFont();
			ImGui::Separator();

			// Name
			ImGui::Text("Name");
			ImGui::SetNextItemWidth(300.0f);
			if (ImGui::InputText("##LayerName", mNoiseLayerNameBuf,
				sizeof(mNoiseLayerNameBuf)))
				CopyFromNameBuf(target.Name, mNoiseLayerNameBuf);

			// Noise Type dropdown
			const char* noiseTypeNames[] = { "Fractal", "Ridged", "Turbulence" };
			ImGui::Text("Type");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::Combo("##NoiseType", &target.GPU.Type,
				noiseTypeNames, 3) && mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::Text("Noise Settings");
			ImGui::PopFont();

			// LOD Activation
			ImGui::Text("LOD Activation");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragInt("##NoiseLOD", &target.GPU.LODActivation,
				1.0f, 0, 25) && mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// Seed (read-only)
			{
				ImGui::Text("Seed");
				ImGui::SetNextItemWidth(180.0f);
				ImGui::BeginDisabled();
				uint32_t seed = target.Seed;
				ImGui::InputScalar("##Seed", ImGuiDataType_U32, &seed);
				ImGui::EndDisabled();
			}

			// Octaves
			ImGui::Text("Octaves");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragInt("##Octaves", &target.GPU.Octaves,
				1.0f, 1, 9) && mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// Frequency
			ImGui::Text("Frequency");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##Frequency", &target.GPU.Frequency,
				0.0001f, 0.0f, FLT_MAX, "%.6f")
				&& mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// Amplitude
			ImGui::Text("Amplitude");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##Amplitude", &target.GPU.Amplitude,
				1.0f, 0.0f, FLT_MAX, "%.1f")
				&& mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// Lacunarity
			ImGui::Text("Lacunarity");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##Lacunarity", &target.GPU.Lacunarity,
				0.01f, 1.0f, 4.0f, "%.2f")
				&& mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// Persistence
			ImGui::Text("Persistence");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##Persistence", &target.GPU.Persistence,
				0.01f, 0.0f, 1.0f, "%.2f")
				&& mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::PushFont(io.Fonts->Fonts[4]);
			ImGui::Text("Blending");
			ImGui::PopFont();

			// Blend Weight
			ImGui::Text("Blend Weight");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##BlendWeight", &target.GPU.BlendWeight,
				0.01f, 0.0f, 2.0f, "%.2f")
				&& mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// Radial Frequency Scale
			ImGui::Text("Radial Freq Scale");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat("##RadialFreqScale", &target.GPU.RadialFreqScale,
				0.01f, 0.05f, 5.0f, "%.2f")
				&& mEditingNoiseLayer && mEditingMaterial)
				mContext->mMaterialsIsDirty = true;

			// ── Close / Add buttons ──
			ImGui::Spacing();
			ImGui::Separator();
			const float btnW = 120.0f;

			if (ImGui::Button("Close##NoiseLayer", ImVec2(btnW, 0.0f)))
			{
				mEditingNoiseLayer = false;
				mEditingNoiseLayerIndex = -1;
				ImGui::CloseCurrentPopup();
			}

			if (!mEditingNoiseLayer)
			{
				ImGui::SameLine();
				if (ImGui::Button("Add##NoiseLayer", ImVec2(btnW, 0.0f)))
				{
					// Generate seed for the new layer
					static std::mt19937 rng{ std::random_device{}() };
					mNoiseLayerDraft.Seed = (uint32_t)rng();

					parentMaterial.NoiseLayers.push_back(mNoiseLayerDraft);
					mEditingNoiseLayer = false;
					mEditingNoiseLayerIndex = -1;
					if (mEditingMaterial)
						mContext->mMaterialsIsDirty = true;
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar(3);
	}

	static const char* meshModeLabels[] =
	{
		"Geometry Clipmapping",
		"Icosphere"
	};

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
				ImGui::PushID("PlanetPopupControls");

				ImGui::Spacing(); 
				ImGui::Indent(10.0f);

				// section header
				ImGui::PushFont(io.Fonts->Fonts[4]);
				bool openDebugData = ImGui::CollapsingHeader("Debug Info");
				ImGui::PopFont();

				if (openDebugData)
				{
					ImGui::Indent();

					if (ImGui::BeginTable("DebugTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
					{
						ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
						ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

						auto meshMode = mContext->GetMeshMode();
						if (meshMode == PlanetMeshMode::Icosphere)
						{
							auto& mesh = mContext->GetIcosphereMesh();

							auto maxSubdivisions = mesh->GetMaxSubdivisions();

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Max Subdivision");
							ImGui::TableSetColumnIndex(1);
							ImGui::AlignTextToFramePadding();
							ImGui::Text("%d", static_cast<int>(maxSubdivisions));
						}

						ImGui::EndTable();
					}

					ImGui::Unindent();
				}

				ImGui::Spacing();            // one line
				ImGui::Spacing();            // another (≈ 10-12 px total)

				// section header
				ImGui::PushFont(io.Fonts->Fonts[4]);
				bool openBaseData = ImGui::CollapsingHeader("Base Data");
				ImGui::PopFont();

				if (openBaseData)
				{
					ImGui::Indent();

					ImGui::BeginGroup();

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

						ImGui::EndTable();
					}

					ImGui::EndGroup();

					ImGui::Unindent();
				}

				ImGui::Spacing();            // one line
				ImGui::Spacing();            // another (≈ 10-12 px total)

				// section header
				ImGui::PushFont(io.Fonts->Fonts[4]);
				bool openPhysicsData = ImGui::CollapsingHeader("Physics");
				ImGui::PopFont();

				if (openPhysicsData)
				{
					ImGui::Indent();

					if (ImGui::BeginTable("PhysicsTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
					{
						ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
						ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Gravity Constant");
						ImGui::TableSetColumnIndex(1);
						float padX = ImGui::GetStyle().CellPadding.x;
						float colW = ImGui::GetColumnWidth();             // total width of column 1
						float fullW = colW - padX * 2.0f;                  // leave padding on both sides
						ImGui::SetNextItemWidth(fullW);
						ImGui::DragFloat("##GravityConstant", &mContext->mGravityConstant, 0.01f, 0.0f, 100.0f, "%.2f");

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::TextWrapped("Surface Air Density (kg/m³)");
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(fullW);
						ImGui::DragFloat("##SurfaceAirDensity", &mContext->mSurfaceAirDensity, 0.001f, 0.0f, 100.0f, "%.3f");

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::TextWrapped("Physics Scale Height(m)");
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(fullW);
						ImGui::DragFloat("##PhysicsScaleHeight", &mContext->mPhysicsScaleHeight, 10.0f, 1.0f, 100000.0f, "%.1f");

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::TextWrapped("Atmosphere Ceiling(m)");
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(fullW);
						ImGui::DragFloat("##AtmosphereCeiling", &mContext->mAtmosphereCeiling, 100.0f, 0.0f, 1000000.0f, "%.0f");

						ImGui::EndTable();
					}

					ImGui::Unindent();
				}

				ImGui::Spacing();            // one line
				ImGui::Spacing();            // another (≈ 10-12 px total)

				// section header
				ImGui::PushFont(io.Fonts->Fonts[4]);
				bool openRenderingData = ImGui::CollapsingHeader("Rendering");
				ImGui::PopFont();

				if (openRenderingData)
				{
					ImGui::Indent();

					if (ImGui::BeginTable("MeshTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
					{
						ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
						ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mesh Type");

						ImGui::TableSetColumnIndex(1);

						float padX = ImGui::GetStyle().CellPadding.x;
						float colW = ImGui::GetColumnWidth();             // total width of column 1
						float fullW = colW - padX * 2.0f;                  // leave padding on both sides
						ImGui::SetNextItemWidth(fullW);

						auto meshMode = mContext->GetMeshMode();
						int currentIndex = static_cast<int>(meshMode);
						if (ImGui::Combo("##Meshype", &currentIndex, meshModeLabels, IM_ARRAYSIZE(meshModeLabels)))
							mContext->SetMeshMode(static_cast<PlanetMeshMode>(currentIndex));

						ImGui::TableNextRow();

						if (meshMode == PlanetMeshMode::GeometryClipmapping)
						{
							static int gridSizes[] = { 33, 65, 129, 257, 513 };
							static int currentGridSize = 129;
							static int currentLOD = 5;

							auto& mesh = mContext->GetGeoClipmapMesh();

							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Grid Size");

							ImGui::TableSetColumnIndex(1);

							ImGui::SetNextItemWidth(fullW);

							std::string gridLabel = std::to_string(mesh->mTempGridSize);          // keep it alive
							if (ImGui::BeginCombo("##GridSize", gridLabel.c_str()))
							{
								for (int i = 0; i < IM_ARRAYSIZE(gridSizes); ++i)
								{ 
									bool selected = (currentGridSize == gridSizes[i]);
									if (ImGui::Selectable(std::to_string(gridSizes[i]).c_str(), selected))
										mesh->mTempGridSize = gridSizes[i];
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
							ImGui::SliderInt("##LOD", &mesh->mTempNumLevels, 1, 30);

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(1);

							const float btnW = 80.0f;
							float indent = fullW - btnW;
							ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

							if (ImGui::Button("Apply", ImVec2(btnW, 0)))
							{
								if (mesh->mTempNumLevels != 0 && mesh->mTempGridSize != 0)
								{
									mesh->mRunOnce = false;

									mesh->mGridSize = mesh->mTempGridSize;
									mesh->mNumLevels = mesh->mTempNumLevels;

									mesh->mGridIsDirty = true;
									mesh->mLODGridIsDirty = true;
									mesh->mRingGridIsDirty = true;

									mesh->Init();

									SceneCamera* camera = mSceneContext->GetMainCamera();
									if (camera)
										mesh->GenerateDistanceLUT(mesh->mNumLevels, mContext->mRadius, camera->GetPerspectiveVerticalFOV(), std::get<0>(mSceneContext->GetViewportSize()));
								}
							}
						}

						if (meshMode == PlanetMeshMode::Icosphere)
						{
							auto& mesh = mContext->GetIcosphereMesh();

							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Backface Culling");

							ImGui::TableSetColumnIndex(1);

							ImGui::SetNextItemWidth(fullW);

							ImGui::Checkbox("##BackfaceCulling", &mesh->mBackfaceCulling);

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Frustum Culling");

							ImGui::TableSetColumnIndex(1);

							ImGui::SetNextItemWidth(fullW);

							ImGui::Checkbox("##FrustumCulling", &mesh->mFrustumCulling);

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Max Subdivision Levels");

							ImGui::TableSetColumnIndex(1);

							ImGui::SetNextItemWidth(fullW);

							if (ImGuiHelpers::DragInt16("##MaxSubdivisionLevels", &mesh->mMaxSubdivisions, 1.0f, 0, mesh->HARDCAPSUBDIVISIONS))
							{
								mesh->mDistanceLUTIsDirty = true;
								mesh->mFaceLevelDotLUTIsDirty = true;
								mesh->mPatchIsDirty = true;
							}

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Patch Levels");

							ImGui::TableSetColumnIndex(1);

							ImGui::SetNextItemWidth(fullW);

							if (ImGuiHelpers::DragInt16("##patchLevels", &mesh->mPatchLevels, 1.0f, 0, 8))
								mesh->mPatchIsDirty = true;

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::TextWrapped("Near Distance(Highest LOD Distance)");

							ImGui::TableSetColumnIndex(1);

							ImGui::SetNextItemWidth(fullW);

							float tempNear = mesh->mNearDistance;
							if (ImGui::DragFloat("##NearDistance", &tempNear, 1.0f, 0.1f, (mesh->mFarDistance-0.1), "%.0f"))
							{
								mesh->mNearDistance = tempNear;
								 
								mesh->mDistanceLUTIsDirty = true;
							}

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();
							ImGui::TextWrapped("Far Distance(Lowest LOD Distance)");

							ImGui::TableSetColumnIndex(1);

							ImGui::SetNextItemWidth(fullW);

							float tempFar = mesh->mFarDistance;
							if (ImGui::DragFloat("##FarDistance", &tempFar, 10000.0f, 0.0f, 10000000.0f, "%.0f"))
							{
								mesh->mFarDistance = tempFar;
								mesh->mDistanceLUTIsDirty = true;
							}
						}

						ImGui::EndTable();
					}

					if (ImGui::BeginTable("MaterialTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
					{
						ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.4f);
						ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, windowSize.x * 0.6f);

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Albedo");

						ImGui::TableSetColumnIndex(1);

						float padX = ImGui::GetStyle().CellPadding.x;
						float colW = ImGui::GetColumnWidth();             // total width of column 1
						float fullW = colW - padX * 2.0f;                  // leave padding on both sides

						// Optional: keep everything aligned and sized predictably
						ImGui::BeginGroup();
						ImGui::PushID("PlanetAlbedo");

						// Thumbnail
						ImGui::PushItemWidth(-1);

						Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

						if (mContext->mBaseHeightMapHandle != AssetHandle(0))
						{
							auto tex = AssetManager::GetAsset<Texture2D>(mContext->mAlbedoTextureHandle);
							if (tex)
								displayTexture = tex.get();
						}

						ImGui::Image(displayTexture->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = mAssetRoot / path;
								filename = completePath.string();

								if (filename)
								{
									RequestTextureImport(*filename, false, [this](AssetHandle handle)
										{
											mContext->mAlbedoTextureHandle = handle;
											auto tex = AssetManager::GetAsset<Texture2D>(handle);
											mContext->mUseAlbedoMap = 1;
											mContext->mAlbedoMapTextureCube = mContext->CreateAlbedoCube(tex.get());
										});
								}
	
							}
							ImGui::EndDragDropTarget();
						}

						// Click to browse
						if (ImGui::IsItemClicked())
						{
							auto texturePath = mAssetRoot / "Textures";
							filename = FileDialogs::OpenFile("", texturePath.string().c_str());

							if (filename)
							{
								RequestTextureImport(*filename, false, [this](AssetHandle handle)
									{
										mContext->mAlbedoTextureHandle = handle;
										auto tex = AssetManager::GetAsset<Texture2D>(handle);
										mContext->mUseAlbedoMap = 1;
										mContext->mAlbedoMapTextureCube = mContext->CreateAlbedoCube(tex.get());
									});
							}
						}

						ImGui::SameLine();

						// Right-side panel next to thumbnail
						// We reserve remaining width in the column
						ImGui::BeginGroup();
						ImGui::SetNextItemWidth(fullW - 64.0f - ImGui::GetStyle().ItemSpacing.x);

						// Use Map checkbox
						bool useMap = mContext->mUseAlbedoMap != 0;
						if (ImGui::Checkbox("Use Map##PlanetAlbedo", &useMap))
							mContext->mUseAlbedoMap = useMap ? 1 : 0;

						// Color (fallback/tint)
						ImGui::SetNextItemWidth(-1);
						ImGui::ColorEdit3("##ColorPlanetAlbedo", &mContext->mAlbedoColor.x);

						ImGui::EndGroup();

						ImGui::PopID();
						ImGui::EndGroup();

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

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Slope Sensitivity");

						ImGui::TableSetColumnIndex(1);

						ImGui::SetNextItemWidth(fullW);

						ImGui::DragFloat("##SlopeSensitivity", &mContext->mSlopeSensitivity, 0.1f, 0.0f, 100.0f, "%.1f");

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Slope Threshold ");

						ImGui::TableSetColumnIndex(1);

						ImGui::SetNextItemWidth(fullW);

						ImGui::DragFloat("##SlopeThreshold", &mContext->mSlopeThreshold, 0.1f, 0.0f, 5.0f, "%.1f");

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Slope Darkening ");

						ImGui::TableSetColumnIndex(1);

						ImGui::SetNextItemWidth(fullW);

						ImGui::DragFloat("##SlopeDarkening ", &mContext->mSlopeDarkening, 0.01f, 0.0f, 1.0f, "%.2f");

						ImGui::EndTable();
					}

					ImGui::Unindent();
				}

				ImGui::Spacing();            // one line
				ImGui::Spacing();            // another (≈ 10-12 px total)

				// section header
				ImGui::PushFont(io.Fonts->Fonts[4]);
				bool openTerrainData = ImGui::CollapsingHeader("Terrain Data");
				ImGui::PopFont();

				if (openTerrainData)
				{
					ImGui::Indent();

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
								mContext->GetGeoClipmapMesh()->GenerateDistanceLUT(mContext->GetGeoClipmapMesh()->mNumLevels, mContext->mRadius, camera->GetPerspectiveVerticalFOV(), std::get<0>(mSceneContext->GetViewportSize()));

							if (mContext->mMeshMode == PlanetMeshMode::Icosphere)
							{
								auto& mesh = mContext->GetIcosphereMesh();
								mesh->mFaceLevelDotLUTIsDirty = true;
								mesh->mHeightMultLUTIsDirty = true;
							}
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
						{
							mContext->mMaxHeight = temp;

							if (mContext->mMeshMode == PlanetMeshMode::Icosphere)
							{
								auto& mesh = mContext->GetIcosphereMesh();
								mesh->mFaceLevelDotLUTIsDirty = true;
								mesh->mHeightMultLUTIsDirty = true;
							}
						}

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

						Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

						if (mContext->mBaseHeightMapHandle != AssetHandle(0))
						{
							auto tex = AssetManager::GetAsset<Texture2D>(mContext->mBaseHeightMapHandle);
							if (tex)
								displayTexture = tex.get();
						}

						ImGui::Image(displayTexture->GetID(), {128.0f, 64.0f});

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = mAssetRoot / path;
								filename = completePath.string();

								if (filename)
								{
									RequestTextureImport(*filename, false, [this](AssetHandle handle)
										{
											mContext->mBaseHeightMapHandle = handle;
											auto tex = AssetManager::GetAsset<Texture2D>(handle);
											mContext->mBaseHeightMapTextureCube = mContext->CreateHeightMapCube(tex.get());
											mContext->mNormalMapTextureCube = mContext->CreateNormalMapCube(mContext->mBaseHeightMapTextureCube.get());
											mContext->mTerrainCubeData = mContext->LoadTerrainDataFromTextureCube();
										});
								}
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							auto texturePath = mAssetRoot / "Textures";
							filename = FileDialogs::OpenFile("", texturePath.string().c_str());

							if (filename)
							{
								RequestTextureImport(*filename, false, [this](AssetHandle handle)
									{
										mContext->mBaseHeightMapHandle = handle;
										auto tex = AssetManager::GetAsset<Texture2D>(handle);
										mContext->mBaseHeightMapTextureCube = mContext->CreateHeightMapCube(tex.get());
										mContext->mNormalMapTextureCube = mContext->CreateNormalMapCube(mContext->mBaseHeightMapTextureCube.get());
										mContext->mTerrainCubeData = mContext->LoadTerrainDataFromTextureCube();
									});
							}
						}

						ImGui::TableSetColumnIndex(1);

						ImGui::TableNextRow();

						DrawTerrainMaterialsListUI();
						DrawTerrainMaterialPopup();

						//// Use the same width logic you already have (fullW)
						//ImGui::SetNextItemWidth(fullW);

						//// --- Visual sizing: show up to 3 items without scrolling ---
						//const float lineH = ImGui::GetTextLineHeightWithSpacing();
						//const float itemH = ImGui::GetFrameHeight();                 // approx height for a button/selectable
						//const float itemPadY = ImGui::GetStyle().ItemSpacing.y;
						//const float childPadY = ImGui::GetStyle().WindowPadding.y;

						//// Height for 3 entries + some padding
						//const float visibleItems = 3.0f;

						//// Button height is driven mostly by FramePadding.y + font height.
						//// A good approximation:
						//const float rowH = ImGui::GetFrameHeight(); // respects current style
						//const float rowGap = 1.0f;                  // match your Dummy() spacing
						//const float innerPadY = 8.0f * 2.0f;        // should match WindowPadding.y * 2

						//float minBoxH = innerPadY + visibleItems * rowH + (visibleItems - 1.0f) * rowGap;

						//// If you want the box to grow with items beyond 3 until scrolling kicks in,
						//// keep it fixed at minBoxH. (Scrolling will handle overflow.)
						//float boxH = minBoxH;

						//ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoMove;

						//// Draw list box
						//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 0.0f));
						//ImGui::BeginChild("##PlanetHeightDetailsBox", ImVec2(fullW, boxH), true, childFlags);
						//ImGui::Dummy(ImVec2(0.0f, 0.5f));

						//int deleteIndex = -1;

						//// Render each detail as a “box” row (clickable)
						//for (int i = 0; i < (int)mContext->mHeightDetails.size(); ++i)
						//{
						//	HeightDetail& d = mContext->mHeightDetails[i];

						//	ImGui::PushID(i);

						//	// Make it look like a boxed item
						//	// Selectable with full width; gives good click behavior
						//	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
						//	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
						//	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));
						//	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

						//	float w = ImGui::GetContentRegionAvail().x;
						//	bool clicked = ImGui::Button(d.Name.c_str(), ImVec2(w, 0.0f));

						//	ImGui::PopStyleVar(3);
						//	ImGui::PopStyleColor(1);

						//	if (ImGui::BeginPopupContextItem("##DetailContext", ImGuiPopupFlags_MouseButtonRight))
						//	{
						//		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
						//		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 6.0f));

						//		if (ImGui::MenuItem("Delete"))
						//			deleteIndex = i;

						//		ImGui::PopStyleVar(2);
						//		ImGui::EndPopup();
						//	}

						//	// Extra spacing between entries
						//	ImGui::Dummy(ImVec2(0.0f, 0.5f));

						//	if (clicked)
						//	{
						//		mEditingDetail = true;
						//		mEditingDetailIndex = i;
						//		mDetailDraft = d;
						//		CopyToNameBuf(mDetailNameBuf, sizeof(mDetailNameBuf), mDetailDraft.Name);

						//		mRequestOpenTerrainDetailPopup = true;
						//	}

						//	ImGui::PopID();

						//	if (deleteIndex != -1)
						//		break;
						//}

						//if (deleteIndex != -1)
						//{
						//	// If you are editing this one (or indices after it), fix state.
						//	if (mEditingDetail)
						//	{
						//		if (mEditingDetailIndex == deleteIndex)
						//		{
						//			mEditingDetail = false;
						//			mEditingDetailIndex = -1;
						//		}
						//		else if (mEditingDetailIndex > deleteIndex)
						//		{
						//			// Vector elements shift left
						//			mEditingDetailIndex--;
						//		}
						//	}

						//	mContext->mHeightDetails.erase(mContext->mHeightDetails.begin() + deleteIndex);

						//	mContext->mHeightDetailsDirty = true;
						//}

						//ImGui::EndChild();
						//ImGui::PopStyleVar();

						//// --- Add button aligned bottom-right of the column ---
						//{
						//	const bool disableAdd = (mContext->mHeightDetails.size() >= 8);

						//	const float btnSize = ImGui::GetFrameHeight(); // square button
						//	float cursorX = ImGui::GetCursorPosX();
						//	float availX = ImGui::GetContentRegionAvail().x;

						//	// Move cursor to the right for the button
						//	ImGui::SetCursorPosX(cursorX + (availX - btnSize - 7.0f));

						//	ImGui::BeginDisabled(disableAdd);
						//	if (ImGui::Button("+", ImVec2(btnSize, btnSize)))
						//	{
						//		// Add new
						//		mEditingDetail = false;
						//		mEditingDetailIndex = -1;

						//		mDetailDraft = HeightDetail{};
						//		static std::mt19937 rng{ std::random_device{}() };
						//		mDetailDraft.Seed = rng();
						//		mContext->BuildPermutationTable(mDetailDraft.Seed, mDetailDraft.Perm);
						//		CopyToNameBuf(mDetailNameBuf, sizeof(mDetailNameBuf), mDetailDraft.Name);

						//		mRequestOpenTerrainDetailPopup = true;
						//	}
						//	ImGui::EndDisabled();
						//}

						//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
						//ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
						//ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

						//if (mRequestOpenTerrainDetailPopup)
						//{
						//	ImGui::OpenPopup("##HeightDetailPopup");
						//	mRequestOpenTerrainDetailPopup = false;
						//}

						//if (ImGui::BeginPopupModal("##HeightDetailPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
						//{
						//	HeightDetail* liveDetail = nullptr;

						//	if (mEditingDetail && mEditingDetailIndex >= 0 && mEditingDetailIndex < (int)mContext->mHeightDetails.size())
						//		liveDetail = &mContext->mHeightDetails[mEditingDetailIndex];

						//	HeightDetail& target =	(liveDetail != nullptr) ? *liveDetail : mDetailDraft;

						//	const char* popupHeader = mEditingDetail ? "Edit Height Detail" : "Add Height Detail";
						//	ImGui::TextUnformatted(popupHeader);
						//	ImGui::Separator();

						//	// Name
						//	ImGui::Text("Name");
						//	ImGui::SetNextItemWidth(360.0f);

						//	if (ImGui::InputText("##HeightDetailName", mDetailNameBuf, sizeof(mDetailNameBuf)))
						//		CopyFromNameBuf(target.Name, mDetailNameBuf);

						//	// LODActivation (uint32_t)
						//	ImGui::Text("LOD Activation");
						//	ImGui::SetNextItemWidth(180.0f);
						//	if (ImGui::DragInt("##LODActivation", &target.GPUSettings.LODActivation, 1.0f, 0, 25) && mEditingDetail)
						//		mContext->mHeightDetailsDirty = true;

						//	// Seed (uint32_t)
						//	{
						//		ImGui::Text("Seed");

						//		ImGui::SetNextItemWidth(180.0f);

						//		ImGui::BeginDisabled(); // ⬅ disables editing
						//		uint32_t seed = target.Seed;
						//		ImGui::InputScalar("##Seed", ImGuiDataType_U32, &seed);
						//		ImGui::EndDisabled();
						//	}

						//	// Octaves (int, >= 1)
						//	ImGui::Text("Octaves");
						//	ImGui::SetNextItemWidth(180.0f);
						//	if(ImGui::DragInt("##Octaves", &target.GPUSettings.Octaves, 1.0f, 1, 9) && mEditingDetail)
						//		mContext->mHeightDetailsDirty = true;

						//	// Frequency (float)
						//	ImGui::Text("Frequency");
						//	ImGui::SetNextItemWidth(180.0f);
						//	if(ImGui::DragFloat("##Frequency", &target.GPUSettings.Frequency, 0.001f, 0.0f) && mEditingDetail)
						//		mContext->mHeightDetailsDirty = true;

						//	// Amplitude (float)
						//	ImGui::Text("Amplitude");
						//	ImGui::SetNextItemWidth(180.0f);
						//	if(ImGui::DragFloat("##Amplitude", &target.GPUSettings.Amplitude, 0.01f, 0.0f, FLT_MAX, "%.2f") && mEditingDetail)
						//		mContext->mHeightDetailsDirty = true;

						//	ImGui::Separator();
						//	const float btnW = 120.0f;

						//	// Cancel always closes
						//	if (ImGui::Button("Close", ImVec2(btnW, 0.0f)))
						//	{
						//		mEditingDetail = false;
						//		mEditingDetailIndex = -1;
						//		ImGui::CloseCurrentPopup();
						//	}

						//	if (!mEditingDetail)
						//	{
						//		ImGui::SameLine();

						//		if (ImGui::Button("Add", ImVec2(btnW, 0.0f)))
						//		{
						//			mContext->mHeightDetails.push_back(mDetailDraft);
						//			mEditingDetail = false;
						//			mEditingDetailIndex = -1;
						//			mContext->mHeightDetailsDirty = true;
						//			ImGui::CloseCurrentPopup();
						//		}
						//	}

						//	ImGui::EndPopup();
						//}

						//ImGui::PopStyleVar(3);

						DrawTerrainObjectsListUI();
						DrawTerrainObjectPopup();

						ImGui::EndTable();
					}

					ImGui::Unindent();
				}

				ImGui::Spacing();            // one line
				ImGui::Spacing();            // another (≈ 10-12 px total)

				// section header
				ImGui::PushFont(io.Fonts->Fonts[4]);
				bool openStarFieldData = ImGui::CollapsingHeader("Star Field");
				ImGui::PopFont();

				if (openStarFieldData)
				{
					ImGui::Indent();

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

						Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

						if (mContext->mStarFieldTexture2DHandle != AssetHandle(0))
						{
							auto tex = AssetManager::GetAsset<Texture2D>(mContext->mStarFieldTexture2DHandle);
							if (tex)
								displayTexture = tex.get();
						}

						ImGui::Image(displayTexture->GetID(), { 128.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = mAssetRoot / path;
								filename = completePath.string();

								if (filename)
								{
									RequestTextureImport(*filename, false, [this](AssetHandle handle)
										{
											mContext->mStarFieldTexture2DHandle = handle;
											auto tex = AssetManager::GetAsset<Texture2D>(handle);
											mContext->mStarFieldTextureCube = Renderer::CreateStarFieldTexture(tex.get());

											Renderer::ResetEnvMapsIBLDone();
										});
								}
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							auto texturePath = mAssetRoot / "Textures";
							filename = FileDialogs::OpenFile("", texturePath.string().c_str());

							if (filename)
							{
								RequestTextureImport(*filename, false, [this](AssetHandle handle)
									{
										mContext->mStarFieldTexture2DHandle = handle;
										auto tex = AssetManager::GetAsset<Texture2D>(handle);
										mContext->mStarFieldTextureCube = Renderer::CreateStarFieldTexture(tex.get());

										Renderer::ResetEnvMapsIBLDone();
									});
							}
						}

						ImGui::TableSetColumnIndex(1);

						ImGui::EndTable();
					}

					ImGui::Unindent();
				}

				ImGui::Spacing();            // one line
				ImGui::Spacing();            // another (≈ 10-12 px total)

				ImGui::PushFont(io.Fonts->Fonts[4]);
				bool openAtmosphericScatteringData = ImGui::CollapsingHeader("Atmospheric Scattering");
				ImGui::PopFont();

				if (openAtmosphericScatteringData)
				{
					ImGui::Indent();

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

						// -------- Rayleigh Scattering Row ----------
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Rayleigh Scattering");

						ImGui::TableSetColumnIndex(1);

						// Right-align small dropdown above the float3
						float comboW = 62.0f;
						float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
						ImGui::SetCursorPosX(rightEdge - comboW);

						ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
						ImGuiHelpers::TinyExponentCombo("##RayleighExp10", mContext->mAtmosphere.RayleighExp10);
						ImGui::PopStyleVar(1);
					 
						ImGui::Dummy(ImVec2(0.0f, 2.0f)); // tiny spacing before the drag control

						ImGui::SetNextItemWidth(fullW);

						ImGuiHelpers::ManualDragFloat3Scaled("##Rayleigh", mContext->mAtmosphere.RayleighScattering, mContext->mAtmosphere.RayleighExp10, 0.01f, 0.0f, mWindow, activeDragArea,	"%.2f",	true, fullW);

						// -------- Mie Scale Height Row ----------
						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mie Scale Height");

						ImGui::TableSetColumnIndex(1);

						ImGui::SetNextItemWidth(fullW);

						ImGui::DragFloat("##MieScaleHeight", &mContext->mAtmosphere.MieScaleHeight, 1.0f, 0.0f, FLT_MAX, "%.0f");

						// -------- Mie Scattering Row ----------
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mie Scattering");

						ImGui::TableSetColumnIndex(1);

						// Right-align small dropdown above the float3
						rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
						ImGui::SetCursorPosX(rightEdge - comboW);

						ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
						ImGuiHelpers::TinyExponentCombo("##MieScatteringExp10", mContext->mAtmosphere.MieScatteringExp10); 
						ImGui::PopStyleVar(1);

						ImGui::Dummy(ImVec2(0.0f, 2.0f)); // tiny spacing before the drag control

						ImGui::SetNextItemWidth(fullW);

						ImGuiHelpers::ManualDragFloat3Scaled("##MieScattering", mContext->mAtmosphere.MieScattering, mContext->mAtmosphere.MieScatteringExp10, 0.01f, 0.0f, mWindow, activeDragArea, "%.2f", true, fullW);

						// -------- Mie Absorption Row ----------
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mie Absorption");

						ImGui::TableSetColumnIndex(1);

						// Right-align small dropdown above the float3
						rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
						ImGui::SetCursorPosX(rightEdge - comboW);

						ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
						ImGuiHelpers::TinyExponentCombo("##MieAbsorptionExp10", mContext->mAtmosphere.MieAbsorptionExp10);
						ImGui::PopStyleVar(1);

						ImGui::Dummy(ImVec2(0.0f, 2.0f)); // tiny spacing before the drag control

						ImGui::SetNextItemWidth(fullW);

						ImGuiHelpers::ManualDragFloat3Scaled("##MieAbsorption", mContext->mAtmosphere.MieAbsorption, mContext->mAtmosphere.MieAbsorptionExp10, 0.01f, 0.0f, mWindow, activeDragArea, "%.2f", true, fullW);

						// -------- Mie Anisotropy Row ----------
						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mie Anisotropy Red");

						ImGui::TableSetColumnIndex(1);

						ImGui::SetNextItemWidth(fullW);

						ImGuiHelpers::ManualDragFloat3("##MieAnisotropy", mContext->mAtmosphere.MieAnisotropy, 0.01f, 0.0f, mWindow, activeDragArea, "%.2f", true);

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

					ImGui::Unindent();
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

		DrawImportTexturePopup();
	}
} 