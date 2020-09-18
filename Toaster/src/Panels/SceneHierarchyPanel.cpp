#include "SceneHierarchyPanel.h"

#include "imgui/imgui.h"

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

	static std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3, DirectX::XMFLOAT3> GetTransformDecomposition(const DirectX::XMMATRIX& transform)
	{
		DirectX::XMFLOAT3 translation, rotation, scale;
		DirectX::XMVECTOR vTranslation, vRotation, vScale;

		DirectX::XMMatrixDecompose(&vScale, &vRotation, &vTranslation, transform);

		DirectX::XMStoreFloat3(&scale, vScale);
		DirectX::XMStoreFloat3(&rotation, vRotation);
		DirectX::XMStoreFloat3(&translation, vTranslation);

		return { translation, rotation, scale };
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		
		ImGuiTreeNodeFlags flags = ((mSelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked()) 
			mSelectionContext = entity;

		if (opened)
			ImGui::TreePop();
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
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

		if (entity.HasComponent<TransformComponent>())
		{
			ImGui::Separator();

			auto& tc = entity.GetComponent<TransformComponent>();
			if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform")) 
			{
				auto [translation, rotation, scale] = GetTransformDecomposition(tc);

				bool updateTransform = false;

				ImGui::Columns(2);
				ImGui::Text("Translation");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);

				if (ImGui::DragFloat3("##translation", (float*)&translation, 0.1f))
					updateTransform = true;

				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Text("Rotation");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);

				if (ImGui::DragFloat3("##rotation", (float*)&rotation, 0.1f))
					updateTransform = true;

				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Text("Scale");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);

				if(ImGui::DragFloat3("##scale", (float*)&scale, 0.1f))
					updateTransform = true;

				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Columns(1);

				if(updateTransform)
					tc.Transform = DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) * 
									DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) * 
									DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);

				ImGui::TreePop();
			}
		}

		if (entity.HasComponent<CameraComponent>())
		{
			ImGui::Separator();

			auto& cc = entity.GetComponent<CameraComponent>();

			const char* projTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProj = projTypeStrings[(int)cc.Camera.GetProjectionType()];
			if (ImGui::TreeNodeEx((void*)typeid(CameraComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera"))
			{

				ImGui::Columns(2);
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

				if (cc.Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float verticalFOV = cc.Camera.GetPerspectiveVerticalFOV();
					if (Property("Vertical FOV", verticalFOV))
						cc.Camera.SetPerspectiveVerticalFOV(verticalFOV);

					float nearClip = cc.Camera.GetPerspectiveNearClip();
					if (Property("Near Clip", nearClip))
						cc.Camera.SetPerspectiveNearClip(nearClip);
					
					float farClip = cc.Camera.GetPerspectiveFarClip();
					if (Property("Far Clip", farClip))
						cc.Camera.SetPerspectiveFarClip(farClip);
				}

				if (cc.Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = cc.Camera.GetOrthographicSize();
					if (Property("Orthographic Size", orthoSize))
						cc.Camera.SetOrthographicSize(orthoSize);

					float nearClip = cc.Camera.GetOrthographicNearClip();
					if (Property("Near Clip", nearClip))
						cc.Camera.SetOrthographicNearClip(nearClip);

					float farClip = cc.Camera.GetOrthographicFarClip();
					if (Property("Far Clip", farClip))
						cc.Camera.SetOrthographicFarClip(farClip);
				}

				ImGui::Columns(1);
				ImGui::TreePop();
			}
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			ImGui::Separator();

			auto& src = entity.GetComponent<SpriteRendererComponent>();
			if (ImGui::TreeNodeEx((void*)typeid(SpriteRendererComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Color"))
			{
				BeginPropertyGrid();

				DirectX::XMFLOAT4 color = src.Color;
				if (Property("Color", color))
					src.Color = color;

				ImGui::Columns(1);
				ImGui::TreePop();
			}
		}
	}
}