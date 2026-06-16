#pragma once

#include "Toast/Core/Log.h"
#include "Toast/Utils/PlatformUtils.h"
#include "MaterialPanel.h"

#include "imgui/imgui.h"

#include "../FontAwesome.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Renderer/Shader.h"

#include <filesystem>

namespace Toast {

	void MaterialPanel::SetContext(const Ref<Material>& context)
	{
		mSelectionContext = context;
	}

	void MaterialPanel::SetProjectPath(const std::filesystem::path& projectPath)
	{
		mAssetRoot = projectPath / "Assets";
	}

	void MaterialPanel::RequestTextureImport(const std::filesystem::path& path, bool defaultSRGB, std::function<void(AssetHandle)> onComplete)
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

	void MaterialPanel::DrawImportTexturePopup()
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

	void MaterialPanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_PAINT_BRUSH" Material");

		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;

		if (ImGui::TreeNodeEx((void*)9817240, treeNodeFlags, "Material Library"))
		{
			static int selected = 0;
			int index = 0;
			AssetManager::Each(AssetType::Material, [&](AssetHandle h, const AssetMetadata&)
				{
					auto mat = AssetManager::GetAsset<Material>(h);
					if (!mat) return;
					if (ImGui::Selectable(mat->GetName().c_str(), selected == index))
					{
						selected = index;
						mSelectionContext = mat;
						mSelectionHandle = h;
					}
					index++;
				});
			ImGui::TreePop();
		}

		if (ImGui::Button("New Material"))
		{
			auto material = CreateRef<Material>("New Material");

			std::filesystem::path relativePath = std::filesystem::path("Materials") / "New Material.tmtl";
			std::filesystem::path fullPath = AssetManager::GetAssetDirectory() / relativePath;
			// optional: append _1/_2 if fullPath already exists

			material->SaveToFile(fullPath);
			AssetHandle handle = AssetManager::ImportAsset(relativePath);
			if (AssetEntry* entry = AssetManager::GetEntry(handle))
				entry->Resource = material;

			mSelectionContext = material;
			mSelectionHandle = handle;
		}

		ImGui::Separator();

		if (mSelectionContext)
			DrawMaterialProperties();

		ImGui::End();

		DrawImportTexturePopup();
	}

	void MaterialPanel::DrawMaterialProperties( )
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
		const ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue;
		const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		bool isDirty = false;

		if (ImGui::TreeNodeEx((void*)9817244, treeNodeFlags, ICON_TOASTER_COG" Material Properties"))
		{
			auto name = mSelectionContext->GetName();
			auto oldName = name;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), name.c_str());
			if (ImGui::InputText("##name", buffer, sizeof(buffer), inputTextFlags))
			{
				std::string newName = std::string(buffer);
				mSelectionContext->SetName(newName);

				// Rename the file to match (extension preserved).
				if (mSelectionHandle)
					AssetManager::RenameAsset(mSelectionHandle, newName + ".tmtl");

				isDirty = true;   // save-on-dirty rewrites the .tmtl (now at its new path) with the new Material: field
			}

			uint64_t imguiPtr = 54332;

			ImGuiTableFlags flags = ImGuiTableFlags_NoBordersInBody;
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			//Albedo
			if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Albedo"))
			{
				ImGui::BeginTable("##table1", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);

				Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

				if (mSelectionContext->GetAlbedoAssetHandle() != AssetHandle(0))
				{
					auto tex = AssetManager::GetAsset<Texture2D>(mSelectionContext->GetAlbedoAssetHandle());
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
									mSelectionContext->SetAlbedolAssetHandle(handle);
								});
						}

						isDirty = true;
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
								mSelectionContext->SetAlbedolAssetHandle(handle);
							});
					}

					isDirty = true;
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::BeginTable("##table2", 2, flags);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				auto useMap = mSelectionContext->GetUseAlbedo();
				if (ImGui::Checkbox("Use##AlbedoMap", &useMap))
				{
					mSelectionContext->SetUseAlbedo(useMap ? 1 : 0);

					isDirty = true;
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				auto& value = mSelectionContext->GetAlbedo();
				if (ImGui::ColorEdit3("color", &value.x))
					isDirty = true;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				float& emission = mSelectionContext->GetEmission();
				ImGui::Text("Emission");
				ImGui::TableSetColumnIndex(1);
				if (ImGui::DragFloat("##emission", &emission))
					isDirty = true;
				ImGui::EndTable();
				ImGui::EndTable();
				ImGui::TreePop();
			}

			//Normal
			if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Normal"))
			{
				ImGui::BeginTable("##table1", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);
				
				Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

				if (mSelectionContext->GetNormalAssetHandle() != AssetHandle(0))
				{
					auto tex = AssetManager::GetAsset<Texture2D>(mSelectionContext->GetNormalAssetHandle());
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
									mSelectionContext->SetNormalAssetHandle(handle);
								});
						}

						isDirty = true;
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
								mSelectionContext->SetNormalAssetHandle(handle);
							});
					}

					isDirty = true;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::BeginTable("##table2", 2, flags);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				auto useMap = mSelectionContext->GetUseNormal();
				if (ImGui::Checkbox("Use##NormalMap", &useMap))
				{
					mSelectionContext->SetUseNormal(useMap ? 1 : 0);

					isDirty = true;
				}

				ImGui::EndTable();
				ImGui::EndTable();
				ImGui::TreePop();
			}

			//Metalness
			if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Metalness/Roughness"))
			{
				ImGui::BeginTable("##table1", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);
				
				Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

				if (mSelectionContext->GetMetalRoughAssetHandle() != AssetHandle(0))
				{
					auto tex = AssetManager::GetAsset<Texture2D>(mSelectionContext->GetMetalRoughAssetHandle());
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
									mSelectionContext->SetMetalRoughAssetHandle(handle);
								});
						}

						isDirty = true;
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
								mSelectionContext->SetMetalRoughAssetHandle(handle);
							});
					}

					isDirty = true;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::BeginTable("##table2", 2, flags);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				auto useMap = mSelectionContext->GetUseMetalRough();

				if (ImGui::Checkbox("Use##MetalRoughMap", &useMap))
				{
					mSelectionContext->SetUseMetalRough(useMap ? 1 : 0);

					isDirty = true;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				auto& metalValue = mSelectionContext->GetMetalness();
				if (ImGui::DragFloat("##metalValue", &metalValue, 0.001f, 0.0f, 1.0f, "%.3f"))
					isDirty = true;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				auto& roughValue = mSelectionContext->GetRoughness();
				if (ImGui::DragFloat("##roughValue", &roughValue, 0.001f, 0.0f, 1.0f, "%.3f"))
					isDirty = true;
				ImGui::EndTable();
				ImGui::EndTable();
				ImGui::TreePop();
			}

			ImGui::TreePop();

			if (isDirty && mSelectionHandle)
			{
				if (AssetEntry* entry = AssetManager::GetEntry(mSelectionHandle))
				{
					auto fullPath = AssetManager::GetAssetDirectory() / entry->Metadata.FilePath;
					mSelectionContext->SaveToFile(fullPath);
				}
			}
		}
	}

}