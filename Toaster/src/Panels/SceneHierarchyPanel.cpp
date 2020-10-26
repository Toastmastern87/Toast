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
		mSelectionContext = {};
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

		if (mSelectionContext)
			DrawComponents(mSelectionContext);

		ImGui::End();
	}

	static uint32_t sCounter = 0;
	static std::string sID;

	static void BeginPropertyGrid()
	{
		sCounter = 0;
		ImGui::Columns(2);
	}

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
			if(opened)
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

	static bool DrawFloatControl(const std::string& label, float& value, float columnWidth = 100.0f, float min = 0.0f, float max = 0.0f, float delta = 0.1f)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1);

		if (ImGui::DragFloat("##label", &value, delta, min, max, "%.2f"))
			modified = true;
		ImGui::PopItemWidth();

		ImGui::Columns(1);

		ImGui::PopID();

		return modified;
	}

	static void DrawFloat3Control(const std::string& label, DirectX::XMFLOAT3& values, float resetValue = 0.0f, float columnWidth = 100.0f) 
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

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
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopStyleColor(3);
		ImGui::PopFont();

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopStyleColor(3);
		ImGui::PopFont();

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopStyleColor(3);
		ImGui::PopFont();

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction) 
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen |ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;

		if (entity.HasComponent<T>())
		{
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
			ImGui::PopStyleVar();
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("ComponentsSettings");
			}


			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentsSettings"))
			{
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				uiFunction(component);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponents<T>();
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
				tag = std::string(buffer);
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
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

		ImGui::PopItemWidth();

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component) 
		{
			DrawFloat3Control("Translation", component.Translation);
			DirectX::XMFLOAT3 rotation = { DirectX::XMConvertToDegrees(component.Rotation.x) , DirectX::XMConvertToDegrees(component.Rotation.y), DirectX::XMConvertToDegrees(component.Rotation.z) };
			DrawFloat3Control("Rotation", rotation);
			component.Rotation = { DirectX::XMConvertToRadians(rotation.x), DirectX::XMConvertToRadians(rotation.y), DirectX::XMConvertToRadians(rotation.z) };
			DrawFloat3Control("Scale", component.Scale, 1.0f);
		});
		
		DrawComponent<PrimitiveMeshComponent>("Primitive Mesh", entity, [](auto& component) 
		{
			const char* primitiveTypeStrings[] = { "None", "Plane", "Cube", "Icosphere", "Grid" };
			const char* currentType = primitiveTypeStrings[(int)component.Mesh->GetPrimitiveType()];

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100.0f);
			ImGui::Text("Primitive Type");
			ImGui::NextColumn();

			ImGui::PushItemWidth(-1);

			if (ImGui::BeginCombo("##primitivetype", currentType))
			{
				for (int type = 0; type < 5; type++)
				{
					bool isSelected = (currentType == primitiveTypeStrings[type]);
					if (ImGui::Selectable(primitiveTypeStrings[type], isSelected))
					{
						currentType = primitiveTypeStrings[type];
						component.Mesh->SetPrimitiveType((Mesh::PrimitiveType)type);
						component.Mesh->CreateFromPrimitive();
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::Columns(1);
		});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component) 
		{
			auto& camera = component.Camera;

			ImGui::Checkbox("Primary", &component.Primary);

			const char* projTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProj = projTypeStrings[(int)camera.GetProjectionType()];

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100.0f);
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
						camera.SetProjectionType((SceneCamera::ProjectionType)type);
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::Columns(1);

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
			{
				float perspectiveVerticalFOV = camera.GetPerspectiveVerticalFOV();
				if (DrawFloatControl("Vertical FOV", perspectiveVerticalFOV))
					camera.SetPerspectiveVerticalFOV(perspectiveVerticalFOV);

				float perspectiveNear = camera.GetPerspectiveNearClip();
				if (DrawFloatControl("Near Clip", perspectiveNear))
					camera.SetPerspectiveNearClip(perspectiveNear);

				float perspectiveFar = camera.GetPerspectiveFarClip();
				if (DrawFloatControl("Far Clip", perspectiveFar, 100.0f, 0.0f, 0.0f, 10.0f))
					camera.SetPerspectiveFarClip(perspectiveFar);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
			{
				float orthoSize = camera.GetOrthographicSize();
				if (DrawFloatControl("Orthographic Size", orthoSize))
					camera.SetOrthographicSize(orthoSize);

				float orthoNear = camera.GetOrthographicNearClip();
				if (DrawFloatControl("Near Clip", orthoNear))
					camera.SetOrthographicNearClip(orthoNear);

				float orthoFar = camera.GetOrthographicFarClip();
				if (DrawFloatControl("Far Clip", orthoFar))
					camera.SetOrthographicFarClip(orthoFar);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			}
		});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component)
		{
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100.0f);
			ImGui::Text("Color");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::ColorEdit4("##color", &component.Color.x);
			ImGui::PopItemWidth();
			ImGui::Columns(1);
		});
	}
}