#include "SceneHierarchyPanel.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Toast/Scene/Components.h"

namespace Toast {

	
	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		mContext = context;
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");

		mContext->mRegistry.each([&](auto entityID) 
		{
			Entity entity{ entityID, mContext.get() };
			DrawEntityNode(entity);
		});

		// Right-click on blank space
		if (ImGui::BeginPopupContextWindow(0, 1, false)) 
		{
			if (ImGui::MenuItem("Create Empty Entity")) 
				mContext->CreateEntity("Empty Entity");

			ImGui::EndPopup();
		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			mSelectionContext = {};

		ImGui::End();

		ImGui::Begin("Properties");

		if (mSelectionContext) {
			DrawComponents(mSelectionContext);

			if (ImGui::Button("Add Component"))
				ImGui::OpenPopup("AddComponentPanel");

			if (ImGui::BeginPopup("AddComponentPanel"))
			{
				if (!mSelectionContext.HasComponent<CameraComponent>())
				{
					if (ImGui::MenuItem("Camera"))
					{
						mSelectionContext.AddComponent<CameraComponent>();
						ImGui::CloseCurrentPopup();
					}
				}

				if (!mSelectionContext.HasComponent<PrimitiveMeshComponent>())
				{
					if (ImGui::MenuItem("Primitive Mesh"))
					{
						mSelectionContext.AddComponent<PrimitiveMeshComponent>(CreateRef<Mesh>());
						ImGui::CloseCurrentPopup();
					}
				}

				if (!mSelectionContext.HasComponent<SpriteRendererComponent>())
				{
					if (ImGui::MenuItem("Sprite Renderer"))
					{
						mSelectionContext.AddComponent<SpriteRendererComponent>();
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}
		}

		ImGui::End();
	}

	static uint32_t sCounter = 0;
	static std::string sID;

	static void BeginPropertyGrid()
	{
		sCounter = 0;
		ImGui::Columns(2);
	}

	static bool Property(const char* label, float& value, float delta = 0.1f)
	{
		bool modified = false;

		ImGui::Text(label);
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		sID.clear();
		sID = "##";
		sID.append(std::to_string(sCounter++));
		if (ImGui::DragFloat(sID.c_str(), &value, delta))
			modified = true;

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return modified;
	}

	static bool Property(const char* label, DirectX::XMFLOAT4& value)
	{
		bool modified = false;

		ImGui::Text(label);
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		sID.clear();
		sID = "##";
		sID.append(std::to_string(sCounter++));
		if (ImGui::ColorEdit4(sID.c_str(), &value.x))
			modified = true;

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return modified;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		
		ImGuiTreeNodeFlags flags = ((mSelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
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
			ImGui::TreePop();

		if (entityDeleted) 
		{
			mContext->DestroyEntity(entity);
			if (mSelectionContext == entity) 
				mSelectionContext = {};
		}
	}

	static void DrawFloat3Control(const std::string& label, DirectX::XMFLOAT3& values, float resetValue = 0.0f, float columnWidth = 100.0f) 
	{
		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			ImGui::Columns(2);
			ImGui::Text("Tag");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
				tag = std::string(buffer);

			ImGui::PopItemWidth();
			ImGui::Columns(1);
		}

		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;

		if (entity.HasComponent<TransformComponent>())
		{
			ImGui::Separator();

			auto& tc = entity.GetComponent<TransformComponent>();

			bool open = ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform");

			if (open)
			{
				DrawFloat3Control("Translation", tc.Translation);
				DirectX::XMFLOAT3 rotation = { DirectX::XMConvertToDegrees(tc.Rotation.x) , DirectX::XMConvertToDegrees(tc.Rotation.y), DirectX::XMConvertToDegrees(tc.Rotation.z) };
				DrawFloat3Control("Rotation", rotation);
				tc.Rotation = { DirectX::XMConvertToRadians(rotation.x), DirectX::XMConvertToRadians(rotation.y), DirectX::XMConvertToRadians(rotation.z) };
				DrawFloat3Control("Scale", tc.Scale, 1.0f);

				ImGui::TreePop();
			}
		}

		if (entity.HasComponent<PrimitiveMeshComponent>())
		{
			ImGui::Separator();

			auto& mc = entity.GetComponent<PrimitiveMeshComponent>();

			const char* meshTypeStrings[] = { "None", "Primitive", "Model" };
			const char* currentType = meshTypeStrings[(int)mc.Mesh->GetType()];

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			bool open = ImGui::TreeNodeEx((void*)typeid(PrimitiveMeshComponent).hash_code(), treeNodeFlags, "Primitive Mesh");
			ImGui::SameLine(ImGui::GetWindowWidth() - 25.0f);
			if (ImGui::Button("+", ImVec2{ 20, 20 }))
			{
				ImGui::OpenPopup("ComponentsSettings");
			}
			ImGui::PopStyleVar();

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentsSettings"))
			{
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				BeginPropertyGrid();

				ImGui::Columns(2);
				ImGui::Text("Type");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);

				const char* primitiveTypeStrings[] = { "None", "Plane", "Cube", "Icosphere", "Grid" };
				const char* currentType = primitiveTypeStrings[(int)mc.Mesh->GetPrimitiveType()];

				if (ImGui::BeginCombo("##primitivetype", currentType))
				{
					for (int type = 0; type < 5; type++)
					{
						bool isSelected = (currentType == primitiveTypeStrings[type]);
						if (ImGui::Selectable(primitiveTypeStrings[type], isSelected))
						{
							currentType = primitiveTypeStrings[type];
							mc.Mesh->SetPrimitiveType((Mesh::PrimitiveType)type);
							mc.Mesh->CreateFromPrimitive();
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::PopItemWidth();
				ImGui::NextColumn();


				ImGui::Columns(1);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponents<PrimitiveMeshComponent>();
		}

		if (entity.HasComponent<CameraComponent>())
		{
			ImGui::Separator();

			bool open = ImGui::TreeNodeEx((void*)typeid(CameraComponent).hash_code(), treeNodeFlags, "Camera");
			
			if (open)
			{
				auto& cc = entity.GetComponent<CameraComponent>();
				auto& camera = cc.Camera;

				ImGui::Checkbox("Primary", &cc.Primary);

				const char* projTypeStrings[] = { "Perspective", "Orthographic" };
				const char* currentProj = projTypeStrings[(int)camera.GetProjectionType()];

				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
				ImGui::SameLine(ImGui::GetWindowWidth() - 25.0f);
				if (ImGui::Button("+", ImVec2{ 20, 20 }))
				{
					ImGui::OpenPopup("ComponentsSettings");
				}
				ImGui::PopStyleVar();

				bool removeComponent = false;
				if (ImGui::BeginPopup("ComponentsSettings"))
				{
					if (ImGui::MenuItem("Remove component"))
						removeComponent = true;

					ImGui::EndPopup();
				}

				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, panelSize.x * 0.35f);
				ImGui::SetColumnWidth(1, panelSize.x * 0.65f);
				ImGui::Text("Projection");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);

				if (ImGui::BeginCombo("##projection", currentProj))
				{
					for (int type = 0; type < 2; type++)
					{
						bool isSelected = (currentProj == projTypeStrings[type]);
						if (ImGui::Selectable(projTypeStrings[type], isSelected))
						{
							currentProj = projTypeStrings[type];
							cc.Camera.SetProjectionType((SceneCamera::ProjectionType)type);
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::PopItemWidth();
				ImGui::NextColumn();

				BeginPropertyGrid();

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float perspectiveVerticalFOV = camera.GetPerspectiveVerticalFOV();
					if (Property("Vertical FOV", perspectiveVerticalFOV))
						camera.SetPerspectiveVerticalFOV(perspectiveVerticalFOV);

					float perspectiveNear = camera.GetPerspectiveNearClip();
					if (Property("Near Clip", perspectiveNear))
						camera.SetPerspectiveNearClip(perspectiveNear);
					
					float perspectiveFar = camera.GetPerspectiveFarClip();
					if (Property("Far Clip", perspectiveFar))
						camera.SetPerspectiveFarClip(perspectiveFar);
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = camera.GetOrthographicSize();
					if (Property("Orthographic Size", orthoSize))
						camera.SetOrthographicSize(orthoSize);

					float orthoNear = camera.GetOrthographicNearClip();
					if (Property("Near Clip", orthoNear))
						camera.SetOrthographicNearClip(orthoNear);

					float orthoFar = camera.GetOrthographicFarClip();
					if (Property("Far Clip", orthoFar))
						camera.SetOrthographicFarClip(orthoFar);

					ImGui::Checkbox("Fixed Aspect Ratio", &cc.FixedAspectRatio);
				}

				ImGui::Columns(1);
				ImGui::TreePop();

				if (removeComponent)
					entity.RemoveComponents<CameraComponent>();
			}
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			ImGui::Separator();

			auto& src = entity.GetComponent<SpriteRendererComponent>();

			bool open = ImGui::TreeNodeEx((void*)typeid(SpriteRendererComponent).hash_code(), treeNodeFlags, "Color");
			ImGui::SameLine(ImGui::GetWindowWidth() - 25.0f);
			if (ImGui::Button("+", ImVec2{ 20, 20 }))
			{
				ImGui::OpenPopup("ComponentsSettings");
			}
			ImGui::PopStyleVar();

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentsSettings"))
			{
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				BeginPropertyGrid();

				DirectX::XMFLOAT4 color = src.Color;
				if (Property("Color", color))
					src.Color = color;

				ImGui::Columns(1);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponents<SpriteRendererComponent>();
		}
	}
}