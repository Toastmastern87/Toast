#pragma once

#include "Toast/Core/Log.h"
#include "Toast/Utils/PlatformUtils.h"
#include "MaterialPanel.h"

#include "imgui/imgui.h"

#include "../FontAwesome.h"

#include "Toast/Renderer/Shader.h"

#include <filesystem>

namespace Toast {

	// Once Toast Engine have "projects", change this
	extern const std::filesystem::path gAssetPath;

	void MaterialPanel::SetContext(const Ref<Material>& context)
	{
		mSelectionContext = context;
	}

	void MaterialPanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_PAINT_BRUSH" Material");

		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;

		if (ImGui::TreeNodeEx((void*)9817240, treeNodeFlags, "Material Library")) 
		{
			std::unordered_map<std::string, Ref<Material>> materials = MaterialLibrary::GetMaterials();

			static int selected = 0;
			int index = 0;
			for (std::pair<std::string, Ref<Material>> material : materials)
			{
				if (ImGui::Selectable(material.second->GetName().c_str(), selected == index))
				{
					selected = index;

					mSelectionContext = material.second;
				}
	
				index++;
			}

			ImGui::TreePop();
		}

		if (ImGui::Button("New Material")) 
		{
			MaterialLibrary::Load();
		}

		ImGui::Separator();

		if (mSelectionContext)
			DrawMaterialProperties();

		ImGui::End();
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
				MaterialLibrary::ChangeName(name, std::string(buffer));
				TOAST_CORE_INFO("Material '%s' changing name to: '%s'", name.c_str(), std::string(buffer).c_str());
				mSelectionContext->SetName(std::string(buffer));

				isDirty = true;
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
				ImGui::Image(mSelectionContext->GetAlbedoTexture()->GetID(), { 64.0f, 64.0f });

				std::optional<std::string> filename;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = std::filesystem::path(gAssetPath) / path;
						filename = completePath.string();

						if (filename)
							mSelectionContext->SetAlbedoTexture(TextureLibrary::LoadTexture2D(*filename));

						isDirty = true;
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");

					if (filename)
						mSelectionContext->SetAlbedoTexture(TextureLibrary::LoadTexture2D(*filename));

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
				ImGui::Image(mSelectionContext->GetNormalTexture()->GetID(), { 64.0f, 64.0f });

				std::optional<std::string> filename;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = std::filesystem::path(gAssetPath) / path;
						filename = completePath.string();

						if (filename)
							mSelectionContext->SetNormalTexture(TextureLibrary::LoadTexture2D(*filename));

						isDirty = true;
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
					if (filename)
						mSelectionContext->SetNormalTexture(TextureLibrary::LoadTexture2D(*filename));

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
				ImGui::Image(mSelectionContext->GetMetalRoughTexture()->GetID(), { 64.0f, 64.0f });

				std::optional<std::string> filename;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = std::filesystem::path(gAssetPath) / path;
						filename = completePath.string();

						if (filename)
							mSelectionContext->SetMetalRoughTexture(TextureLibrary::LoadTexture2D(*filename));

						isDirty = true;
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
					if (filename)
						mSelectionContext->SetMetalRoughTexture(TextureLibrary::LoadTexture2D(*filename));

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

			if (isDirty)
				MaterialSerializer::Serialize(mSelectionContext);	
		}
	}

}