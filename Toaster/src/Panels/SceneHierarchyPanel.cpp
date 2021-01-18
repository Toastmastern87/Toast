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
		ImGui::Begin("Hierarchy");

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

			ImGui::Separator();
			if (ImGui::MenuItem("Create Cube"))
				mContext->CreateCube("Cube");

			if (ImGui::MenuItem("Create Sphere"))
				mContext->CreateSphere("Sphere");

			ImGui::EndPopup();
		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			mSelectionContext = {};

		ImGui::End();

		ImGui::Begin("Properties");

		if (mSelectionContext)
			DrawComponents(mSelectionContext);

		ImGui::End();

		ImGui::Begin("Statistics");

		ImGui::Text("FPS: %d", (int)mContext->mStats.FPS);
		ImGui::Text("Vertex Count: %d", (int)mContext->mStats.VerticesCount);

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

	static bool DrawIntControl(const std::string& label, int& value, float columnWidth = 100.0f, int min = 0, int max = 0)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1);

		if (ImGui::SliderInt("##label", &value, min, max))
			modified = true;

		ImGui::PopItemWidth();
		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
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

	static float CalculateDelta(float value)
	{
		if (value >= 1000.0f || value <= -1000.0f)
			return 10.0f;
		else if(value >= 100.0f || value <= -100.0f)
			return 1.0f;
		else if (value >= 10.0f || value <= -10.0f) 
			return 0.1f;
		else if (value >= 1.0f || value <= -1.0f)
			return 0.01f;
		else
			return 0.01f;
	}

	static bool DrawFloat3Control(const std::string& label, DirectX::XMFLOAT3& values, float resetValue = 0.0f, float columnWidth = 100.0f) 
	{
		bool modified = false;

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
		if (ImGui::DragFloat("##X", &values.x, CalculateDelta(values.x), 0.0f, 0.0f, (values.x >= 1000.0f || values.x <= -1000.0f) ? "%.0f" : "%.1f"))
			modified = true;
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
		if(ImGui::DragFloat("##Y", &values.y, CalculateDelta(values.y), 0.0f, 0.0f, (values.y >= 1000.0f || values.y <= -1000.0f) ? "%.0f" : "%.1f"))
			modified = true;
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
		if(ImGui::DragFloat("##Z", &values.z, CalculateDelta(values.z), 0.0f, 0.0f, (values.z >= 1000.0f || values.z <= -1000.0f) ? "%.0f" : "%.1f"))
			modified = true;
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return modified;
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
				uiFunction(component, entity);
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

			if (!mSelectionContext.HasComponent<MeshComponent>())
			{
				if (ImGui::MenuItem("Primitive Mesh"))
				{
					mSelectionContext.AddComponent<MeshComponent>(CreateRef<Mesh>());
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mSelectionContext.HasComponent<PlanetComponent>())
			{
				if (ImGui::MenuItem("Planet"))
				{
					mSelectionContext.AddComponent<PlanetComponent>();
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

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component, Entity entity) 
		{
			float fov = 45.0f;

			DrawFloat3Control("Translation", component.Translation);
			DirectX::XMFLOAT3 rotation = { DirectX::XMConvertToDegrees(component.Rotation.x) , DirectX::XMConvertToDegrees(component.Rotation.y), DirectX::XMConvertToDegrees(component.Rotation.z) };
			DrawFloat3Control("Rotation", rotation);
			component.Rotation = { DirectX::XMConvertToRadians(rotation.x), DirectX::XMConvertToRadians(rotation.y), DirectX::XMConvertToRadians(rotation.z) };
			bool scaleModified = DrawFloat3Control("Scale", component.Scale, 1.0f);

			// If the component has a planet recalculate the Distance LUT
			if (entity.HasComponent<PlanetComponent>() && scaleModified)
			{
				auto view = entity.mScene->mRegistry.view<TransformComponent, CameraComponent>();
				for (auto entity : view)
				{
					auto& camera = view.get<CameraComponent>(entity);

					if (camera.Primary)
						fov = camera.Camera.GetPerspectiveVerticalFOV();
				}

				auto& pc = entity.GetComponent<PlanetComponent>();

				PlanetSystem::GenerateDistanceLUT(pc.MorphData.DistanceLUT, component.Scale.x, fov, (float)entity.mScene->mViewportWidth, 200.0f, 8);
				PlanetSystem::GenerateFaceDotLevelLUT(pc.FaceLevelDotLUT, component.Scale.x, 8, pc.PlanetData.maxAltitude.x);
			}
		});
		
		DrawComponent<MeshComponent>("Mesh", entity, [](auto& component, Entity entity) 
		{
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100.0f);
			ImGui::Text("Material");
			ImGui::NextColumn();

			ImGui::PushItemWidth(-1);

			std::unordered_map<std::string, Ref<Material>> materials = MaterialLibrary::GetMaterials();
			Ref<Material> currentMaterial = component.Mesh->GetMaterial();
			if (ImGui::BeginCombo("##material", currentMaterial->GetName().c_str()))
			{
				for (auto& material : materials)
				{
					bool isSelected = (currentMaterial->GetName() == material.first);
					if (ImGui::Selectable(material.first.c_str(), isSelected))
						component.Mesh->SetMaterial(MaterialLibrary::Get(material.first));

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::Columns(1);
		});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component, Entity entity)
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

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component, Entity entity)
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

		DrawComponent<PlanetComponent>("Planet", entity, [](auto& component, Entity entity)
		{
				DirectX::XMVECTOR cameraPos = { 0.0f, 0.0f, 0.0f }, cameraRot = { 0.0f, 0.0f, 0.0f }, cameraScale = { 0.0f, 0.0f, 0.0f };
				int patchLevels = component.PatchLevels;
				int subdivions = component.Subdivisions;
				float fov = 45.0f;

				if (DrawIntControl("Patch levels", patchLevels, 100.0f, 1, 6) || DrawIntControl("Subdivisions", subdivions, 100.0f, 0, 8) || DrawFloatControl("Max Altitude(km)", component.PlanetData.maxAltitude.x) || DrawFloatControl("Min Altitude(km)", component.PlanetData.minAltitude.x) || DrawFloatControl("Radius(km)", component.PlanetData.radius.x))
				{
					component.PatchLevels = patchLevels;
					component.Subdivisions = subdivions;
					MeshComponent mc = entity.GetComponent<MeshComponent>();
					TransformComponent tc = entity.GetComponent<TransformComponent>();

					auto view = entity.mScene->mRegistry.view<TransformComponent, CameraComponent>();
					for (auto entity : view)
					{
						auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

						if (camera.Primary)
						{
							DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, transform.GetTransform());
							fov = camera.Camera.GetPerspectiveVerticalFOV();
						}
					}

					PlanetSystem::GenerateDistanceLUT(component.MorphData.DistanceLUT, tc.Scale.x, fov, (float)entity.mScene->mViewportWidth, 200.0f, 8);
					PlanetSystem::GenerateFaceDotLevelLUT(component.FaceLevelDotLUT, tc.Scale.x, 8, component.PlanetData.maxAltitude.x);
					PlanetSystem::GeneratePatchGeometry(mc.Mesh->mPlanetVertices, mc.Mesh->mIndices, component.PatchLevels);
					PlanetSystem::GeneratePlanet(tc.GetTransform(), mc.Mesh->mPlanetFaces, mc.Mesh->mPlanetPatches, component.MorphData.DistanceLUT, component.FaceLevelDotLUT, cameraPos, component.Subdivisions);

					mc.Mesh->InitPlanet();
					mc.Mesh->AddSubmesh(mc.Mesh->mIndices.size());
				}
		});
	}

}