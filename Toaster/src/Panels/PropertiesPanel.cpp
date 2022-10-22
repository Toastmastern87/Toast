#include "PropertiesPanel.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Toast/Core/UUID.h"

#include "Toast/Script/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"

#include "Toast/Scene/Components.h"

#include "Toast/Utils/PlatformUtils.h"

#include "../FontAwesome.h"

#include "imgui/imgui.h"

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

	static uint32_t sCounter = 0;
	static char sIDBuffer[16];

	static bool DrawFloatControl(const std::string& label, float& value, float imGuiTableWidth = 90.0f, float min = 0.0f, float max = 0.0f, float delta = 0.1f)
	{
		bool modified = false;
		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushID(label.c_str());

		ImGui::BeginTable("##table2", 2, flags);
		ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, imGuiTableWidth);
		ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - imGuiTableWidth);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text(label.c_str());
		ImGui::TableSetColumnIndex(1);

		ImGui::PushItemWidth(-1);

		if (ImGui::DragFloat("##label", &value, delta, min, max, "%.2f"))
			modified = true;
		ImGui::PopItemWidth();

		ImGui::EndTable();
		ImGui::PopID();

		return modified;
	}

	static float CalculateDelta(float value)
	{
		if (value >= 1000.0f || value <= -1000.0f)
			return 10.0f;
		else if (value >= 100.0f || value <= -100.0f)
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
		{
			values.x = resetValue;
			modified = true;
		}

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
		{
			values.y = resetValue;
			modified = true;
		}
		ImGui::PopStyleColor(3);
		ImGui::PopFont();

		ImGui::SameLine();
		if (ImGui::DragFloat("##Y", &values.y, CalculateDelta(values.y), 0.0f, 0.0f, (values.y >= 1000.0f || values.y <= -1000.0f) ? "%.0f" : "%.1f"))
			modified = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
		{
			values.z = resetValue;
			modified = true;
		}
		ImGui::PopStyleColor(3);
		ImGui::PopFont();

		ImGui::SameLine();
		if (ImGui::DragFloat("##Z", &values.z, CalculateDelta(values.z), 0.0f, 0.0f, (values.z >= 1000.0f || values.z <= -1000.0f) ? "%.0f" : "%.1f"))
			modified = true;
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return modified;
	}

	static bool DrawFloat2Control(const std::string& label, DirectX::XMFLOAT2& values, float resetValue = 0.0f, float columnWidth = 100.0f)
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
		{
			values.x = resetValue;
			modified = true;
		}

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
		{
			values.y = resetValue;
			modified = true;
		}
		ImGui::PopStyleColor(3);
		ImGui::PopFont();

		ImGui::SameLine();
		if (ImGui::DragFloat("##Y", &values.y, CalculateDelta(values.y), 0.0f, 0.0f, (values.y >= 1000.0f || values.y <= -1000.0f) ? "%.0f" : "%.1f"))
			modified = true;
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return modified;
	}

	PropertiesPanel::PropertiesPanel(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel)
	{
		SetContext(context, sceneHierarchyPanel);
	}

	void PropertiesPanel::SetContext(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel)
	{
		mSceneHierarchyPanel = sceneHierarchyPanel;
		mScene = mSceneHierarchyPanel->GetContext();

		mContext = context;
	}

	void PropertiesPanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_WRENCH" Properties");

		mContext = mSceneHierarchyPanel->GetSelectedEntity();
		mScene = mSceneHierarchyPanel->GetContext();

		if (mContext)
			DrawComponents(mContext);

		ImGui::End();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, Scene* scene, UIFunction uiFunction)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;

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
			if (ImGui::Button(ICON_TOASTER_TRASH_O"", ImVec2{ lineHeight, lineHeight }))
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
				uiFunction(component, entity, scene);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponents<T>();
		}
	}

	void PropertiesPanel::DrawComponents(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy_s(buffer, tag.c_str(), sizeof(buffer));
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
				tag = std::string(buffer);
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			if (!mContext.HasComponent<CameraComponent>())
			{
				if (ImGui::MenuItem("Camera"))
				{
					mContext.AddComponent<CameraComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<MeshComponent>())
			{
				if (ImGui::MenuItem("Mesh"))
				{
					mContext.AddComponent<MeshComponent>(CreateRef<Mesh>());
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<PlanetComponent>())
			{
				if (ImGui::MenuItem("Planet"))
				{
					mContext.AddComponent<PlanetComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<SpriteRendererComponent>())
			{
				if (ImGui::MenuItem("Sprite Renderer"))
				{
					mContext.AddComponent<SpriteRendererComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<DirectionalLightComponent>())
			{
				if (ImGui::MenuItem("Directional Light"))
				{
					mContext.AddComponent<DirectionalLightComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<SkyLightComponent>())
			{
				if (ImGui::MenuItem("Sky Light"))
				{
					mContext.AddComponent<SkyLightComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<ScriptComponent>())
			{
				if (ImGui::MenuItem("Script"))
				{
					mContext.AddComponent<ScriptComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<RigidBodyComponent>())
			{
				if (ImGui::MenuItem("Rigid Body"))
				{
					mContext.AddComponent<RigidBodyComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<SphereColliderComponent>())
			{
				if (ImGui::MenuItem("Sphere Collider"))
				{
					mContext.AddComponent<SphereColliderComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<TerrainColliderComponent>())
			{
				if (ImGui::MenuItem("Terrain Collider"))
				{
					mContext.AddComponent<TerrainColliderComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<UIPanelComponent>())
			{
				if (ImGui::MenuItem("UI Panel"))
				{
					mContext.AddComponent<UIPanelComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<UITransformComponent>())
			{
				if (ImGui::MenuItem("UI Transform"))
				{
					mContext.AddComponent<UITransformComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		ImGui::TextDisabled("UUID: %llx", entity.GetComponent<IDComponent>().ID);

		DrawComponent<TransformComponent>(ICON_TOASTER_ARROWS_ALT" Transform", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				float fov = 45.0f;
				DirectX::XMFLOAT3 translationFloat3, scaleFloat3;
				DirectX::XMVECTOR translation, scale, rotation;
				DirectX::XMMATRIX cameraTransform;
				DirectX::XMVECTOR cameraPos, cameraRot, cameraScale;

				DirectX::XMMatrixDecompose(&scale, &rotation, &translation, component.Transform);
				DirectX::XMStoreFloat3(&translationFloat3, translation);
				DirectX::XMStoreFloat3(&scaleFloat3, scale);

				bool updateTransform = false;

				updateTransform |= DrawFloat3Control("Translation", translationFloat3);
				updateTransform |= DrawFloat3Control("Rotation", component.RotationEulerAngles);
				updateTransform |= DrawFloat3Control("Scale", scaleFloat3, 1.0f);

				if (updateTransform) 
					component.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scaleFloat3.x, scaleFloat3.y, scaleFloat3.z)
						* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(component.RotationEulerAngles.x), DirectX::XMConvertToRadians(component.RotationEulerAngles.y), DirectX::XMConvertToRadians(component.RotationEulerAngles.z))))
						* DirectX::XMMatrixTranslation(translationFloat3.x, translationFloat3.y, translationFloat3.z);

				// If the component has a planet recalculate the Distance LUT
				if (entity.HasComponent<PlanetComponent>() && updateTransform)
				{
					DirectX::XMVECTOR cameraForward = { 0.0f, 0.0f, 1.0f };

					auto view = entity.mScene->mRegistry.view<TransformComponent, CameraComponent>();
					for (auto entity : view)
					{
						auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
						cameraTransform = transform.Transform;

						if (camera.Primary) 
						{
							fov = camera.Camera.GetPerspectiveVerticalFOV();

							DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);
						}
						else 
						{
							cameraPos = { 0.0f, 0.0f, 0.0f };
						}
					}

					auto& pc = entity.GetComponent<PlanetComponent>();

					PlanetSystem::GenerateDistanceLUT(pc.DistanceLUT, 8.0f);
					PlanetSystem::GenerateFaceDotLevelLUT(pc.FaceLevelDotLUT, DirectX::XMVectorGetX(scale), 8.0f, pc.PlanetData.maxAltitude);
					PlanetSystem::GenerateHeightMultLUT(pc.Mesh->mPlanetFaces, pc.HeightMultLUT, DirectX::XMVectorGetX(scale), 8, pc.PlanetData.maxAltitude, component.Transform);

					PlanetSystem::GeneratePlanet(scene->GetFrustum(), component.Transform, pc.Mesh->GetPlanetFaces(), pc.Mesh->GetPlanetPatches(), pc.DistanceLUT, pc.FaceLevelDotLUT, pc.HeightMultLUT, cameraPos, cameraPos, pc.Subdivisions, scene->mSettings.BackfaceCulling, scene->mSettings.FrustumCulling);

					pc.Mesh->InvalidatePlanet(false);
				}
			});

		DrawComponent<MeshComponent>(ICON_TOASTER_CUBE" Mesh", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("##MeshTable", 3, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Mesh ");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (!component.Mesh->GetFilePath().empty())
					ImGui::InputText("##meshfilepath", (char*)component.Mesh->GetFilePath().c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##meshfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openmesh"))
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.gltf", "..\\Toaster\\assets\\meshes\\");
					if (filepath) 
					{
						auto& tag = entity.GetComponent<TagComponent>().Tag;
						auto id = entity.GetComponent<IDComponent>().ID;
						if (tag == "Empty Entity") 
						{
							std::string newTag = *filepath;
							std::size_t found = newTag.find_last_of("/\\");
							newTag = newTag.substr(found + 1);
							found = newTag.find_last_of(".\\");
							tag = newTag.substr(0, found);
						}

						component.Mesh = CreateRef<Mesh>(*filepath);
					}
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Material ");
				ImGui::TableSetColumnIndex(1);

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

				ImGui::EndTable();
			});

		DrawComponent<CameraComponent>(ICON_TOASTER_CAMERA" Camera", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				auto& camera = component.Camera;

				ImGui::Checkbox("Primary", &component.Primary);

				const char* projTypeStrings[] = { "Perspective", "Orthographic" };
				const char* currentProj = projTypeStrings[(int)camera.GetProjectionType()];

				ImGui::BeginTable("CameraTable", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::Text("Projection");
				ImGui::TableSetColumnIndex(1);
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

				ImGui::EndTable();

				ImGui::Columns(1);

				float perspectiveVerticalFOV = camera.GetPerspectiveVerticalFOV();
				if (DrawFloatControl("Vertical FOV", perspectiveVerticalFOV, 90.0f))
					camera.SetPerspectiveVerticalFOV(perspectiveVerticalFOV);

				float n = camera.GetNearClip();
				if (DrawFloatControl("Near Clip", n, 90.0f))
					camera.SetNearClip(n);

				float f = camera.GetFarClip();
				if (DrawFloatControl("Far Clip", f, 90.0f, 0.0f, 0.0f, 10.0f))
					camera.SetFarClip(f);

				float orthoWidth = camera.GetOrthographicWidth();
				float orthoHeight = camera.GetOrthographicHeight();
				if (DrawFloatControl("Ortho Width", orthoWidth, 90.0f) || DrawFloatControl("Ortho Height", orthoHeight, 90.0f))
					camera.SetOrthographicSize(orthoWidth, orthoHeight);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("PlanetComponentTable", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##color", &component.Color.x);

				ImGui::EndTable();
			});

		DrawComponent<PlanetComponent>(ICON_TOASTER_GLOBE" Planet", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				DirectX::XMVECTOR cameraPos = { 0.0f, 0.0f, 0.0f }, cameraRot = { 0.0f, 0.0f, 0.0f }, cameraScale = { 0.0f, 0.0f, 0.0f };
				int patchLevels = component.PatchLevels;
				int subdivions = component.Subdivisions;
				float fov = 45.0f;
				bool modified = false;

				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("PlanetComponentTable", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Patch levels");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::SliderInt("##Patchlevels", &patchLevels, 1, 8))
					modified = true;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Subdivisions");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::SliderInt("##Subdivisions", &subdivions, 0, 8))
					modified = true;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Max Alt(km)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##MaxAlt", &component.PlanetData.maxAltitude, 0.1f, 0.0f, 0.0f, "%.2f"))
					modified = true;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Min Alt(km)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##MinAltitude", &component.PlanetData.minAltitude, 0.1f, 0.0f, 0.0f, "%.2f"))
					modified = true;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Radius(km)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##Radius", &component.PlanetData.radius, 0.1f, 0.0f, 0.0f, "%.2f"))
					modified = true;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Gravitational acceleration(m/s^2)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##GravAcc", &component.PlanetData.gravAcc, 0.1f, 0.0f, 0.0f, "%.2f"))
					modified = true;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Atmosphere)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::Checkbox("##Atmosphere", &component.PlanetData.atmosphereToggle);

				if (component.PlanetData.atmosphereToggle)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Atmosphere\nHeight");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##AtmosphereHeight", &component.PlanetData.atmosphereHeight, 0.1f, 0.0f, 1000.0f, "%.1f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("In Scattering\nPoints");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragInt("##InScatteringPoints", &component.PlanetData.inScatteringPoints, 1.0f, 1, 40);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Optical\nDepth Points");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragInt("##OpticalDepthPoints", &component.PlanetData.opticalDepthPoints, 1.0f, 1, 40);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Mie\nAnisotropy");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##mieAnisotropy", &component.PlanetData.mieAnisotropy, 0.001f, -1.0f, 1.0f, "%.3f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ray\nScale Height");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##rayScaleHeight", &component.PlanetData.rayScaleHeight, 0.1f, 0.0f, 40.0f, "%.1f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Mie\nScale Height");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##mieScaleHeight", &component.PlanetData.mieScaleHeight, 0.1f, 0.0f, 40.0f, "%.1f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ray Scattering\nCoefficient Red");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##rayScatteringnCoefficientRed", &component.PlanetData.rayBaseScatteringCoefficient.x, 0.00001f, 0.0f, 1.0f, "%.7f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ray Scattering\nCoefficient Green");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##rayScatteringnCoefficientGreen", &component.PlanetData.rayBaseScatteringCoefficient.y, 0.00001f, 0.0f, 1.0f, "%.7f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ray Scattering\nCoefficient Blue");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##rayScatteringnCoefficientBlue", &component.PlanetData.rayBaseScatteringCoefficient.z, 0.00001f, 0.0f, 1.0f, "%.7f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Mie Scattering");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##mieScattering", &component.PlanetData.mieBaseScatteringCoefficient, 0.0001f, 0.0f, 1.0f, "%.4f");
				}

				ImGui::EndTable();

				if (modified)
				{
					component.PatchLevels = patchLevels;
					component.Subdivisions = subdivions;
					TransformComponent tc = entity.GetComponent<TransformComponent>();

					auto view = entity.mScene->mRegistry.view<TransformComponent, CameraComponent>();
					for (auto entity : view)
					{
						auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

						if (camera.Primary)
						{
							DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, transform.Transform);
							fov = camera.Camera.GetPerspectiveVerticalFOV();
						}
					}

					DirectX::XMVECTOR scale, rotation, translation, cameraForward = { 0.0f, 0.0f, 1.0f };
					DirectX::XMMatrixDecompose(&scale, &rotation, &translation, tc.Transform);

					DirectX::XMVECTOR rotationMatrix = DirectX::XMQuaternionRotationMatrix(tc.Transform);
					cameraForward = rotationMatrix * cameraForward;

					PlanetSystem::GeneratePatchGeometry(component.Mesh->mPlanetVertices, component.Mesh->mIndices, component.PatchLevels);
					PlanetSystem::GenerateDistanceLUT(component.DistanceLUT, 8);
					PlanetSystem::GeneratePlanet(scene->GetFrustum(), tc.Transform, component.Mesh->mPlanetFaces, component.Mesh->mPlanetPatches, component.DistanceLUT, component.FaceLevelDotLUT, component.HeightMultLUT, cameraPos, cameraForward, component.Subdivisions, scene->mSettings.BackfaceCulling, scene->mSettings.FrustumCulling);

					component.Mesh->InvalidatePlanet(true);
				}
			});

		DrawComponent<DirectionalLightComponent>(ICON_TOASTER_SUN_O" Directional Light", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("DirectionalLightTable", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Radiance");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit3("##Radiance", &component.Radiance.x);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Intensity");
				ImGui::TableSetColumnIndex(1);
				ImGui::DragFloat("##label", &component.Intensity, 0.01f, 0.0f, 25.0f, "%.2f");

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Sun Disc");
				ImGui::TableSetColumnIndex(1);
				ImGui::Checkbox("##checkbox", &component.SunDisc);
				ImGui::EndTable();
			});

		DrawComponent<SkyLightComponent>(ICON_TOASTER_CLOUD" Sky Light", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("SkyLightTable", 3, flags);

				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("File Path");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (!component.SceneEnvironment.FilePath.empty())
					ImGui::InputText("##envfilepath", (char*)component.SceneEnvironment.FilePath.c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##envfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openenv"))
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.png", "..\\Toaster\\assets\\textures\\");
					if (filepath)
						component.SceneEnvironment = Environment::Load(*filepath);
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Intensity");
				ImGui::TableSetColumnIndex(1);
				ImGui::DragFloat("##label", &component.Intensity, 0.01f, 0.0f, 5.0f, "%.2f");
				ImGui::EndTable();
			});

		DrawComponent<ScriptComponent>(ICON_TOASTER_CODE" Script", entity, mScene, [=](auto& sc, Entity entity, Scene* scene)
			{
				std::string oldName = sc.ModuleName;

				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("ScriptTable", 3, flags);

				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Script");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (!sc.ModuleName.empty())
					ImGui::InputText("##scriptfilepath", (char*)sc.ModuleName.c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##scriptfilepath", (char*)"", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openscript"))
				{
					std::optional<std::string> file = FileDialogs::OpenFile("*.cs", "..\\Toaster\\assets\\scripts\\");
					if (file)
					{
						std::filesystem::path filename = *file;
						std::string filenameString = filename.filename().string();
						if (filenameString.find('.') != std::string::npos)
							filenameString = filenameString.substr(0, filenameString.find_last_of('.'));

						sc.ModuleName = filenameString;
					}

					// Shutdown old script
					if (ScriptEngine::ModuleExists(oldName))
						ScriptEngine::ShutdownScriptEntity(entity.mScene->GetUUID(), entity.GetComponent<IDComponent>().ID, oldName);

					if (ScriptEngine::ModuleExists(sc.ModuleName))
						ScriptEngine::InitScriptEntity(entity);
				}

				ImGui::EndTable();

				if (ScriptEngine::ModuleExists(sc.ModuleName))
				{
					sCounter = 0;

					ImGui::BeginTable("ScriptPropertiesTable", 2, flags);
					ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
					ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);

					EntityInstanceData& entityInstanceData = ScriptEngine::GetEntityInstanceData(entity.GetSceneUUID(), entity.GetComponent<IDComponent>().ID);
					auto& modulePropertiesMap = entityInstanceData.ModulePropertyMap;
					if (modulePropertiesMap.find(sc.ModuleName) != modulePropertiesMap.end())
					{
						auto& publicProperties = modulePropertiesMap.at(sc.ModuleName);
						for (auto& [name, prop] : publicProperties)
						{
							bool isRuntime = mScene->mIsPlaying && prop.IsRuntimeAvailable();
							switch (prop.Type)
							{
							case PropertyType::Float:
								float value = isRuntime ? prop.GetRuntimeValue<float>() : prop.GetStoredValue<float>();
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text(name.c_str());
								ImGui::TableSetColumnIndex(1);

								sIDBuffer[0] = '#';
								sIDBuffer[1] = '#';
								memset(sIDBuffer + 2, 0, 14);
								itoa(sCounter++, sIDBuffer + 2, 16);

								if (ImGui::DragFloat(sIDBuffer, &value, 0.2f, 0.0f, 0.0f, "%.1f"))
								{
									if (isRuntime)
										prop.SetRuntimeValue(value);
									else
										prop.SetStoredValue(value);
								}

								break;
							}
						}
					}
					ImGui::EndTable();
				}
			});

		DrawComponent<RigidBodyComponent>(ICON_TOASTER_HAND_ROCK_O" Rigid Body", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("RigidBody", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Center of Mass");
				ImGui::TableSetColumnIndex(1);
				DrawFloat3Control("CenterOfMass", component.CenterOfMass);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Mass (kg)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				float mass = 1.0f / component.InvMass;
				ImGui::DragFloat("##label", &mass, 0.1f, 0.0f, 60000.0f, "%.1f");
				component.InvMass = 1.0f / mass;

				ImGui::EndTable();
			});

		DrawComponent<SphereColliderComponent>(ICON_TOASTER_CIRCLE_O" Sphere Collider", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::Checkbox("Render Collider", &component.RenderCollider);

				ImGui::BeginTable("SphereCollider", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Radius");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##label", &component.Radius, 0.1f, 0.0f, 600.0f, "%.4f");
				ImGui::EndTable();
			});

		DrawComponent<TerrainColliderComponent>(ICON_TOASTER_GLOBE" Terrain Collider", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("##TerrainColliderTable", 3, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Height Map ");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (!component.FilePath.empty())
					ImGui::InputText("##heightmapfilepath", (char*)component.FilePath.c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##heightmapfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openheightmapfilepath"))
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.png", "..\\Toaster\\assets\\textures\\");
					if (filepath)
					{
						auto& tag = entity.GetComponent<TagComponent>().Tag;
						auto id = entity.GetComponent<IDComponent>().ID;
						if (tag == "Empty Entity")
						{
							std::string newTag = *filepath;
							std::size_t found = newTag.find_last_of("/\\");
							newTag = newTag.substr(found + 1);
							found = newTag.find_last_of(".\\");
							tag = newTag.substr(0, found);
						}

						component.FilePath = *filepath;
						component.TerrainData = PhysicsEngine::LoadTerrainData(component.FilePath.c_str());
					}
				}
				ImGui::EndTable();
			});


		DrawComponent<UITransformComponent>(ICON_TOASTER_ARROWS_ALT" UI Transform", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				DirectX::XMFLOAT2 positionFloat2, sizeFloat2;
				DirectX::XMVECTOR position, size, rotation;

				DirectX::XMMatrixDecompose(&size, &rotation, &position, component.Transform);
				DirectX::XMStoreFloat2(&positionFloat2, position);
				DirectX::XMStoreFloat2(&sizeFloat2, size);

				bool updateTransform = false;

				positionFloat2.y *= -1.0f;

				updateTransform |= DrawFloat2Control("Position", positionFloat2);
				updateTransform |= DrawFloat2Control("Size", sizeFloat2, 1.0f);

				if (updateTransform)
					component.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(sizeFloat2.x, sizeFloat2.y, 1.0f)
					* DirectX::XMMatrixTranslation(positionFloat2.x, -positionFloat2.y, 0.0f);
			});

		DrawComponent<UIPanelComponent>(ICON_TOASTER_SQUARE_O" UI Panel", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("UIPanelComponentTable", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##color", &component.Color.x);

				ImGui::EndTable();
			});
	}

}