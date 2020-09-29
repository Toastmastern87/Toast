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

		if (mSelectionContext) {
			DrawComponents(mSelectionContext);

			if (ImGui::Button("Add Component"))
				ImGui::OpenPopup("AddComponentPanel");

			if (ImGui::BeginPopup("AddComponentPanel"))
			{
				if (!mSelectionContext.HasComponent<MeshComponent>())
				{
					if (ImGui::Button("Mesh"))
					{
						mSelectionContext.AddComponent<MeshComponent>(CreateRef<Mesh>());
						ImGui::CloseCurrentPopup();
					}
				}
				if (!mSelectionContext.HasComponent<CameraComponent>())
				{
					if (ImGui::Button("Camera"))
					{
						mSelectionContext.AddComponent<CameraComponent>();
						ImGui::CloseCurrentPopup();
					}
				}

				if (!mSelectionContext.HasComponent<SpriteRendererComponent>())
				{
					if (ImGui::Button("Sprite Renderer"))
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

		if (entity.HasComponent<TransformComponent>())
		{
			ImGui::Separator();

			auto& tc = entity.GetComponent<TransformComponent>();

			if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform")) 
			{
				auto [translation, rotation, scale] = GetTransformDecomposition(tc);

				bool updateTransform = false;

				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, panelSize.x * 0.35f);
				ImGui::SetColumnWidth(1, panelSize.x * 0.65f);
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

		if (entity.HasComponent<MeshComponent>())
		{
			ImGui::Separator();

			auto& mc = entity.GetComponent<MeshComponent>();

			const char* meshTypeStrings[] = { "None", "Primitive", "Model" };
			const char* currentType = meshTypeStrings[(int)mc.Mesh->GetType()];

			if (ImGui::TreeNodeEx((void*)typeid(MeshComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Mesh"))
			{
				if (mc.Mesh->GetType() == Mesh::MeshType::NONE)
				{
					ImGui::Columns(2);
					ImGui::Text("Type");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);

					if (ImGui::BeginCombo("##type", currentType))
					{
						for (int type = 0; type < 3; type++)
						{
							bool isSelected = (currentType == meshTypeStrings[type]);
							if (ImGui::Selectable(meshTypeStrings[type], isSelected))
							{
								currentType = meshTypeStrings[type];
								mc.Mesh->SetType((Mesh::MeshType)type);
							}
							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					ImGui::PopItemWidth();
					ImGui::NextColumn();
				}

				BeginPropertyGrid();

				if (mc.Mesh->GetType() == Mesh::MeshType::PRIMITIVE)
				{
					ImGui::Columns(2);
					ImGui::Text("Primitive type");
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
				}

				if (mc.Mesh->GetType() == Mesh::MeshType::MODEL)
				{
				}

				if (mc.Mesh->GetType() == Mesh::MeshType::NONE)
				{
					ImGui::Columns(1);
					ImGui::Text("Choose a type!");
				}

				ImGui::Columns(1);
				ImGui::TreePop();
			}
		}

		if (entity.HasComponent<CameraComponent>())
		{
			ImGui::Separator();

			auto& cc = entity.GetComponent<CameraComponent>();
			auto& camera = cc.Camera;

			ImGui::Checkbox("Primary", &cc.Primary);

			const char* projTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProj = projTypeStrings[(int)camera.GetProjectionType()];
			if (ImGui::TreeNodeEx((void*)typeid(CameraComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera"))
			{
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
					float verticalFOV = camera.GetPerspectiveVerticalFOV();
					if (Property("Vertical FOV", verticalFOV))
						camera.SetPerspectiveVerticalFOV(verticalFOV);

					float nearClip = camera.GetPerspectiveNearClip();
					if (Property("Near Clip", nearClip))
						camera.SetPerspectiveNearClip(nearClip);
					
					float farClip = camera.GetPerspectiveFarClip();
					if (Property("Far Clip", farClip))
						camera.SetPerspectiveFarClip(farClip);
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = camera.GetOrthographicSize();
					if (Property("Orthographic Size", orthoSize))
						camera.SetOrthographicSize(orthoSize);

					float nearClip = camera.GetOrthographicNearClip();
					if (Property("Near Clip", nearClip))
						camera.SetOrthographicNearClip(nearClip);

					float farClip = camera.GetOrthographicFarClip();
					if (Property("Far Clip", farClip))
						camera.SetOrthographicFarClip(farClip);

					ImGui::Checkbox("Fixed Aspect Ratio", &cc.FixedAspectRatio);
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