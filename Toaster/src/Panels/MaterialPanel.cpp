#pragma once

#include "Toast/Core/Log.h"
#include "Toast/Utils/PlatformUtils.h"
#include "MaterialPanel.h"

#include "imgui/imgui.h"

#include "Toast/Renderer/Shader.h"

namespace Toast {

	void MaterialPanel::SetContext(const Ref<Material>& context)
	{
		mSelectionContext = context;
	}

	void MaterialPanel::OnImGuiRender()
	{
		ImGui::Begin("Material");

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

		if (ImGui::TreeNodeEx((void*)9817244, treeNodeFlags, "Material Properties"))
		{
			auto name = mSelectionContext->GetName();

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), name.c_str());
			if (ImGui::InputText("##name", buffer, sizeof(buffer), inputTextFlags))
			{
				MaterialLibrary::ChangeName(name, std::string(buffer));
				mSelectionContext->SetName(std::string(buffer));
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
						TOAST_CORE_INFO("CHANGING SHADER TO: {0}", mSelectionContext->GetShader()->GetName());
					}


					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::Columns(1);

			for (auto& resource : mSelectionContext->GetTextureBindings())
			{
				std::string textureName = mSelectionContext->GetShader()->GetResourceName(Shader::BindingType::Texture, resource.BindSlot, resource.ShaderType);

				if (ImGui::TreeNodeEx(resource.Texture ? (void*)resource.Texture->GetID() : (void*)54332, treeNodeFlags, textureName.c_str()))
				{
					ImGui::Image(resource.Texture ? (void*)resource.Texture->GetID() : (void*)TextureLibrary::Get("assets/textures/Checkerboard.png")->GetID(), { 64.0f, 64.0f });

					if (ImGui::IsItemClicked())
					{
						std::string filename = FileDialogs::OpenFile("");
						if (filename != "")
							mSelectionContext->SetTexture(resource.BindSlot, resource.ShaderType, TextureLibrary::LoadTexture2D(filename));
					}

					ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}
	}

}