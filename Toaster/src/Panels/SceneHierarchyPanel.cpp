#include "SceneHierarchyPanel.h"

#include "imgui/imgui_internal.h"

#include "Toast/Core/UUID.h"

#include "Toast/Scene/Components.h"
#include "Toast/Scene/Prefab.h"

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

	static char prefabName[128] = "NewPrefab";

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

		ImVec4 titleBarColor = ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive];
		ImVec4 titleBarHoveredColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
		ImVec4 buttonColor = ImGui::GetStyle().Colors[ImGuiCol_Button];

		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, titleBarColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, titleBarHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, titleBarColor);

		ImGui::SetNextWindowBgAlpha(1.0f);

		// Prefab name popup
		if (mShowPrefabPopup)
		{
			ImGui::SetNextWindowPos(ImVec2(mPrefabPopupPos.x + 50, mPrefabPopupPos.y + 50), ImGuiCond_Appearing);
			ImGui::OpenPopup("Prefab Name");
		}

		if (ImGui::BeginPopupModal("Prefab Name", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Enter Prefab Name:");
			ImGui::InputText("##PrefabName", prefabName, IM_ARRAYSIZE(prefabName));

			// Spacing before buttons
			ImGui::Dummy(ImVec2(0.0f, 10.0f));

			// Center buttons
			float buttonWidth = 80.0f; // Adjust as needed
			float spacing = 10.0f;     // Space between buttons
			float totalWidth = (buttonWidth * 2) + spacing;
			float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			if (ImGui::Button("Ok", ImVec2(buttonWidth, 0)))
			{
				auto& pc = mPrefabEntity.AddComponent<PrefabComponent>();
				std::string prefabNameStr = prefabName;
				pc.PrefabHandle = prefabNameStr;
				PrefabLibrary::Load(mPrefabEntity, prefabNameStr); // Assuming Load() can take a name
				mShowPrefabPopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
			{
				mShowPrefabPopup = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(4);

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
					auto mc = newEntity.AddComponent<MeshComponent>(CreateRef<Mesh>("../Toaster/assets/meshes/Cube.gltf"));

					SetSelectedEntity(newEntity);
				}

				if (ImGui::MenuItem("Sphere"))
				{
					auto newEntity = mContext->CreateEntity("Sphere");
					auto& tc = newEntity.GetComponent<TransformComponent>();
					auto mc = newEntity.AddComponent<MeshComponent>(CreateRef<Mesh>("..\\Toaster\\assets\\meshes\\Sphere.gltf"));

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

		bool isRenaming = (mEntityBeingRenamed == entity);
		
		if (isRenaming)
		{
			// We can indent manually so it lines up similarly to siblings
			ImGui::Indent();

			// On the first frame we show this InputText, set keyboard focus
			if (mSetRenameFocus)
			{
				ImGui::SetKeyboardFocusHere();  // Focus this widget
				mSetRenameFocus = false;
			}

			// Draw the rename text box. Since we’re not calling TreeNodeEx at all,
			// there is no arrow or highlight.
			if (ImGui::InputText(
				"##RenameEntityInput",
				mRenameBuffer,
				IM_ARRAYSIZE(mRenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
			{
				// Pressed ENTER
				entity.GetComponent<TagComponent>().Tag = mRenameBuffer;
				mEntityBeingRenamed = {};
			}

			// If the user clicks away or it deactivates, commit the rename
			if (!ImGui::IsItemActive() && ImGui::IsItemDeactivated())
			{
				entity.GetComponent<TagComponent>().Tag = mRenameBuffer;
				mEntityBeingRenamed = {};
			}

			ImGui::Unindent();
		}
		else
		{
			if (entity.HasComponent<PrefabComponent>())
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.5f, 1.0f, 1.0f)); 

			ImGuiTreeNodeFlags flags = ((mSelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
			flags |= entity.GetComponent<RelationshipComponent>().Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : 0;
			flags |= ImGuiTreeNodeFlags_SpanAvailWidth;


			bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				SetSelectedEntity(entity);
			else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
			{
				// Enter rename mode for this Entity
				mEntityBeingRenamed = entity;
				// Copy current tag into buffer
				memset(mRenameBuffer, 0, sizeof(mRenameBuffer));
				strncpy(mRenameBuffer, tag.c_str(), sizeof(mRenameBuffer) - 1);

				mSetRenameFocus = true;
			}

			if (entity.HasComponent<PrefabComponent>())
				ImGui::PopStyleColor(); // Reset text color

			bool entityDeleted = false;
			if (ImGui::BeginPopupContextItem("EntityContextMenu"))
			{
				if (ImGui::MenuItem("Create Child Entity"))
				{
					Entity childEntity = mContext->CreateEntity("Child Entity");

					mContext->AddChildEntity(childEntity, entity);
				}
				
				if (entity.HasComponent<PrefabComponent>())
				{
					auto& pc = entity.GetComponent<PrefabComponent>();

					if (ImGui::MenuItem("Update Prefab"))
						PrefabLibrary::Update(entity, pc.PrefabHandle);
				}
				else 
				{
					if (ImGui::MenuItem("Create Prefab"))
					{
						mPrefabEntity = entity;
						mShowPrefabPopup = true;
						mPrefabPopupPos = ImGui::GetMousePos();
						memset(prefabName, 0, sizeof(prefabName)); // Clear the name input
						strcpy(prefabName, "NewPrefab"); // Set default name
					}

				}

				if(ImGui::MenuItem("Delete Entity"))
					entityDeleted = true;

				ImGui::EndPopup();
			}

			if (opened)
			{
				if (entity.HasComponent<PrefabComponent>())
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.5f, 1.0f, 1.0f));

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
				//TOAST_CORE_INFO("entity.Children().size: %d", entity.Children().size());
				for (auto child : entity.Children())
				{
					Entity e = mContext->FindEntityByUUID(child);
					if (e.HasComponent<TagComponent>())
						DrawEntityNode(e);
				}

				ImGui::TreePop();

				if (entity.HasComponent<PrefabComponent>())
					ImGui::PopStyleColor(); // Reset text color
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

}