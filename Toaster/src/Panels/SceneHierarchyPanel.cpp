#include "SceneHierarchyPanel.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Toast/Core/UUID.h"

#include "Toast/Scene/Components.h"

#include "Toast/Script/ScriptEngine.h"

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

		mContext->mRegistry.each([&](auto entityID)
			{
				Entity entity{ entityID, mContext.get() };
				// TODO change to ID Component once that one is added
				if (entity.HasComponent<TagComponent>())
					DrawEntityNode(entity);
			});

		// Right-click on blank space
		if (ImGui::BeginPopupContextWindow(0, 1, false))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
				mContext->CreateEntity("Empty Entity");

			ImGui::Separator();
			if (ImGui::BeginMenu("3D"))
			{
				if (ImGui::MenuItem("Cube")) 
				{
					auto newEntity = mContext->CreateEntity("Cube");
					auto mc = newEntity.AddComponent<MeshComponent>(CreateRef<Mesh>("..\\Toaster\\assets\\meshes\\Cube.fbx"));

					SetSelectedEntity(newEntity);
				}

				if (ImGui::MenuItem("Sphere"))
				{
					auto newEntity = mContext->CreateEntity("Sphere");
					auto mc = newEntity.AddComponent<MeshComponent>(CreateRef<Mesh>("..\\Toaster\\assets\\meshes\\Sphere.fbx"));

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
	}

	static std::string sID;

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = ((mSelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked()) 
			mSelectionContext = entity;

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (opened)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			bool opened = ImGui::TreeNodeEx((void*)9817239, flags, tag.c_str());
			if (opened)
				ImGui::TreePop();

			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			mContext->DestroyEntity(entity);
			if (mSelectionContext == entity)
				mSelectionContext = {};
		}
	}

}