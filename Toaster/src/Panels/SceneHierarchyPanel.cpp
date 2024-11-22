#include "SceneHierarchyPanel.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Toast/Core/UUID.h"

#include "Toast/Scene/Components.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Renderer/MeshFactory.h"

#include "Toast/Utils/PlatformUtils.h"

#include "../FontAwesome.h"

#include <filesystem>
#include <string>
#include <cstring>

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
	#define _CRT_SECURE_NO_WARNINGS
#endif

namespace Toast {

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		mContext = context;
		mSelectionContext = {};
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_SITEMAP" Hierarchy");

		for (auto entity : mContext->mRegistry.view<IDComponent, RelationshipComponent>())
		{
			Entity e{ entity, mContext.get() };

			if(e.GetParentUUID() == 0)
				DrawEntityNode(e);
		}

		// Right-click on blank space
		if (ImGui::BeginPopupContextWindow(0, 1, false))
		{
			if (ImGui::MenuItem("Create Empty Entity")) 
			{
				auto newEntity = mContext->CreateEntity("Empty Entity");

				SetSelectedEntity(newEntity);
			}

			ImGui::Separator();
			if (ImGui::BeginMenu("3D"))
			{
				if (ImGui::MenuItem("Cube")) 
				{
					auto newEntity = mContext->CreateEntity("Cube");
					auto& tc = newEntity.GetComponent<TransformComponent>();
					auto mc = newEntity.AddComponent<MeshComponent>(CreateRef<Mesh>("../Toaster/assets/meshes/Cube.gltf", false));

					SetSelectedEntity(newEntity);
				}

				if (ImGui::MenuItem("Sphere"))
				{
					auto newEntity = mContext->CreateEntity("Sphere");
					auto& tc = newEntity.GetComponent<TransformComponent>();
					auto mc = newEntity.AddComponent<MeshComponent>(CreateRef<Mesh>("..\\Toaster\\assets\\meshes\\Sphere.gltf", false));

					SetSelectedEntity(newEntity);
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			mSelectionContext = {};

		ImGui::End();
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
	{
		mSelectionContext = entity;
		mContext->SetSelectedEntity(entity);
	}

	static std::string sID;

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		
		ImGuiTreeNodeFlags flags = ((mSelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= entity.GetComponent<RelationshipComponent>().Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : 0;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked())
			SetSelectedEntity(entity);

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem()) 
		{
			if (ImGui::MenuItem("Create Child Entity")) 
			{
				Entity childEntity = mContext->CreateEntity("Child Entity");

				mContext->AddChildEntity(childEntity, entity);
			}
				
			else if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (opened)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
			//TOAST_CORE_INFO("entity.Children().size: %d", entity.Children().size());
			for (auto child : entity.Children())
			{
				Entity e = mContext->FindEntityByUUID(child);
				if (e.HasComponent<TagComponent>())
					DrawEntityNode(e);
			}

			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			if (entity.GetParentUUID()) 
				mContext->FindEntityByUUID(entity.GetParentUUID()).RemoveChild(entity);		

			if (!entity.Children().empty()) 
			{
				for (auto child : entity.Children()) 
				{
					auto childEntity = mContext->FindEntityByUUID(child);
					mContext->DestroyEntity(childEntity);
					if (mSelectionContext == childEntity)
						mSelectionContext = {};
				}
			}

			mContext->DestroyEntity(entity);
			if (mSelectionContext == entity)
				mSelectionContext = {};
		}
	}

}