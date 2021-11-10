#pragma once

#include "Toast/Core/Log.h"
#include "Toast/Utils/PlatformUtils.h"
#include "MaterialPanel.h"

#include "imgui/imgui.h"

#include "../FontAwesome.h"

#include "Toast/Renderer/Shader.h"

namespace Toast {

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
				
			Ref<Shader> currentShader;
			std::vector<std::string> shaders = ShaderLibrary::GetShaderList();

			currentShader = mSelectionContext->GetShader();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100.0f);
			ImGui::Text("Shader");
			ImGui::NextColumn();

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

			ImGui::Columns(1);

			uint64_t imguiPtr = 54332;
			for (auto& resource : mSelectionContext->GetTextureBindings())
			{
				std::string textureName = mSelectionContext->GetShader()->GetResourceName(Shader::BindingType::Texture, resource.BindSlot, resource.ShaderType);

				if (textureName != "IrradianceTexture" && textureName != "RadianceTexture" && textureName != "SpecularBRDFLUT") {
					// Removes the texture part of the string
					std::size_t pos = textureName.find("Texture");
					std::string rawName = textureName.substr(0, pos);
					std::string toggleName = rawName;
					toggleName.append("TexToggle");

					//TOAST_CORE_INFO("textureName: %s", textureName.c_str());
					//TOAST_CORE_INFO("rawName: %s", rawName.c_str());
					//TOAST_CORE_INFO("toggleName: %s", toggleName.c_str());

					if (ImGui::TreeNodeEx((void*)imguiPtr, treeNodeFlags, textureName.c_str()))
					{
						ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

						if (ImGui::IsItemClicked())
						{
							std::optional<std::string> filename = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");
							if (filename)
								mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(*filename));

							isDirty = true;
						}

						if(textureName != "HeightMapTexture" && textureName != "CraterMapTexture")
						{
							auto useMap = mSelectionContext->Get<bool>(toggleName);
							ImGui::SameLine();
							if (ImGui::Checkbox("Use##Map", &useMap))
							{
								mSelectionContext->Set<int>(toggleName, useMap ? 1 : 0);

								isDirty = true;
							}

							if (rawName == "Albedo")
							{
								ImGui::PushItemWidth(-1);
								ImGui::SameLine();
								auto& value = mSelectionContext->GetFloat3(rawName);
								if (ImGui::ColorEdit3("color", &value.x))
									isDirty = true;
								float& emission = mSelectionContext->GetFloat("Emission");
								ImGui::Text("Emission");
								if(ImGui::DragFloat("##emission", &emission))
									isDirty = true;

							}
							else if (rawName == "Roughness" || rawName == "Metalness")
							{
								ImGui::PushItemWidth(-1);
								ImGui::SameLine();
								auto& value = mSelectionContext->GetFloat(rawName);
								if (ImGui::DragFloat("##value", &value, 0.001f, 0.0f, 1.0f, "%.3f"))
									isDirty = true;
							}
						}

						ImGui::TreePop();
					}
				}

				// 10 being the max number of textures that the material panel can show
				imguiPtr++;;
			}

			ImGui::TreePop();

			if (isDirty)
				MaterialSerializer::Serialize(mSelectionContext);
		}
	}

}