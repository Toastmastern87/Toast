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
				
			Shader* currentShader;
			std::vector<std::string> shaders = ShaderLibrary::GetShaderList();

			currentShader = mSelectionContext->GetShader();

			ImGui::BeginTable("##table2", 2, flags);
			ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 100.0f);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Shader");
			ImGui::TableSetColumnIndex(1);

			ImGui::PushItemWidth(-1);

			if (ImGui::BeginCombo("##shader", currentShader->GetName().c_str()))
			{
				for (auto& shader : shaders)
				{
					bool isSelected = (currentShader->GetName() == shader);
					if (ImGui::Selectable(shader.c_str(), isSelected)) 
					{
						mSelectionContext->SetShader(ShaderLibrary::Get(shader));
						TOAST_CORE_INFO("Material '%s' changing shader to: '%s'", mSelectionContext->GetName().c_str(), mSelectionContext->GetShader()->GetName().c_str());

						isDirty = true;
					}


					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::EndTable();

			uint64_t imguiPtr = 54332;
			for (auto& resource : mSelectionContext->GetTextureBindings())
			{
				std::string textureName = mSelectionContext->GetShader()->GetResourceName(Shader::BindingType::Texture, resource.BindSlot, resource.ShaderType);

				ImGuiTableFlags flags = ImGuiTableFlags_NoBordersInBody;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				//Albedo
				if (textureName == "AlbedoTexture") 
				{
					if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Albedo"))
					{
						ImGui::BeginTable("##table1", 2, flags);
						ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
						ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::PushItemWidth(-1);
						ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = std::filesystem::path(gAssetPath) / path;
								filename = completePath.string();

								if (filename)
									mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

								isDirty = true;
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");

							if (filename)
								mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

							isDirty = true;
						}
						ImGui::TableSetColumnIndex(1);
						ImGui::BeginTable("##table2", 2, flags);
						ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
						ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						auto useMap = mSelectionContext->Get<bool>("AlbedoTexToggle");
						if (ImGui::Checkbox("Use##AlbedoMap", &useMap))
						{
							mSelectionContext->Set<int>("AlbedoTexToggle", useMap ? 1 : 0);

							isDirty = true;
						}
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-1);
						auto& value = mSelectionContext->GetFloat3("Albedo");
						if (ImGui::ColorEdit3("color", &value.x))
							isDirty = true;
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						float& emission = mSelectionContext->GetFloat("Emission");
						ImGui::Text("Emission");
						ImGui::TableSetColumnIndex(1);
						if (ImGui::DragFloat("##emission", &emission))
							isDirty = true;
						ImGui::EndTable();
						ImGui::EndTable();
						ImGui::TreePop();
					}
				}

				//Normal
				if (textureName == "NormalTexture")
				{
					if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Normal"))
					{
						ImGui::BeginTable("##table1", 2, flags);
						ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
						ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::PushItemWidth(-1);

						ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = std::filesystem::path(gAssetPath) / path;
								filename = completePath.string();

								if (filename)
									mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

								isDirty = true;
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
							if (filename)
								mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

							isDirty = true;
						}

						ImGui::TableSetColumnIndex(1);
						ImGui::BeginTable("##table2", 2, flags);
						ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
						ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						auto useMap = mSelectionContext->Get<bool>("NormalTexToggle");
						if (ImGui::Checkbox("Use##NormalMap", &useMap))
						{
							mSelectionContext->Set<int>("NormalTexToggle", useMap ? 1 : 0);

							isDirty = true;
						}

						ImGui::EndTable();
						ImGui::EndTable();
						ImGui::TreePop();
					}
				}

				//Metalness
				if (textureName == "MetalnessTexture")
				{
					if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Metalness"))
					{
						ImGui::BeginTable("##table1", 2, flags);
						ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
						ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::PushItemWidth(-1);

						ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = std::filesystem::path(gAssetPath) / path;
								filename = completePath.string();

								if (filename)
									mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

								isDirty = true;
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
							if (filename)
								mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

							isDirty = true;
						}

						ImGui::TableSetColumnIndex(1);
						ImGui::BeginTable("##table2", 2, flags);
						ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
						ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						auto useMap = mSelectionContext->Get<bool>("MetalnessTexToggle");

						if (ImGui::Checkbox("Use##MetalnessMap", &useMap))
						{
							mSelectionContext->Set<int>("MetalnessTexToggle", useMap ? 1 : 0);

							isDirty = true;
						}

						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-1);
						auto& value = mSelectionContext->GetFloat("Metalness");
						if (ImGui::DragFloat("##value", &value, 0.001f, 0.0f, 1.0f, "%.3f"))
							isDirty = true;

						ImGui::EndTable();
						ImGui::EndTable();
						ImGui::TreePop();
					}
				}

				//Roughness
				if (textureName == "RoughnessTexture")
				{
					if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Roughness"))
					{
						ImGui::BeginTable("##table1", 2, flags);
						ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
						ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::PushItemWidth(-1);

						ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = std::filesystem::path(gAssetPath) / path;
								filename = completePath.string();

								if (filename)
									mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

								isDirty = true;
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
							if (filename)
								mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

							isDirty = true;
						}

						ImGui::TableSetColumnIndex(1);
						ImGui::BeginTable("##table2", 2, flags);
						ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
						ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						auto useMap = mSelectionContext->Get<bool>("RoughnessTexToggle");
						if (ImGui::Checkbox("Use##RoughnessMap", &useMap))
						{
							mSelectionContext->Set<int>("RoughnessTexToggle", useMap ? 1 : 0);

							isDirty = true;
						}

						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-1);
						auto& value = mSelectionContext->GetFloat("Roughness");
						if (ImGui::DragFloat("##value", &value, 0.001f, 0.0f, 1.0f, "%.3f"))
							isDirty = true;

						ImGui::EndTable();
						ImGui::EndTable();
						ImGui::TreePop();
					}
				}

				//Height Map
				if (textureName == "HeightMapTexture")
				{
					if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Height Map"))
					{
						ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = std::filesystem::path(gAssetPath) / path;
								filename = completePath.string();

								if (filename)
									mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

								isDirty = true;
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
							if (filename)
								mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

							isDirty = true;
						}

						ImGui::TreePop();
					}
				}

				//Crater Map
				if (textureName == "CraterMapTexture")
				{
					if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, "Crater Map"))
					{
						ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filename;

						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = std::filesystem::path(gAssetPath) / path;
								filename = completePath.string();

								if (filename)
									mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

								isDirty = true;
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
							if (filename)
								mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

							isDirty = true;
						}

						ImGui::TreePop();
					}
				}
			}

			ImGui::TreePop();

			if (isDirty)
				MaterialSerializer::Serialize(mSelectionContext);
		}
	}

}