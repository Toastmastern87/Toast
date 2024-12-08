#include "PropertiesPanel.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Toast/Renderer/Renderer.h"

#include "Toast/Core/UUID.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"
#include "Toast/Physics/Bounds.h"

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

	// Once Toast Engine have "projects", change this
	extern const std::filesystem::path gAssetPath;

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

	static const char* GetPrecision(float value) 
	{
		if (value >= 10.0f)
			return "%.0f";
		else if (value >= 1.0f && value < 10.0f)
			return "%.1f";
		else if (value >= 0.1f && value < 1.0f)
			return "%.2f";
		else if (value >= 0.01f && value < 0.1f)
			return "%.3f";
		else
			return "%.4f";
	}

	static bool DrawDouble3Control(const std::string& label, Vector3& values, double resetValue = 0.0, float columnWidth = 100.0f)
	{
		bool modified = false;
		float temp;

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
		temp = static_cast<float>(values.x);
		if (ImGui::DragFloat("##X", &temp, 0.01f, 0.0f, 0.0f, GetPrecision(temp)))
		{
			values.x = static_cast<double>(temp);
			modified = true;
		}
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
		temp = static_cast<float>(values.y);
		if (ImGui::DragFloat("##Y", &temp, 0.01f, 0.0f, 0.0f, GetPrecision(temp)))
		{
			values.y = static_cast<double>(temp);
			modified = true;
		}
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
		temp = static_cast<float>(values.z);
		if (ImGui::DragFloat("##Z", &temp, 0.01f, 0.0f, 0.0f, GetPrecision(temp)))
		{
			values.z = static_cast<double>(temp);
			modified = true;
		}
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return modified;
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
		if (ImGui::DragFloat("##X", &values.x, 0.01f, 0.0f, 0.0f, GetPrecision(values.x)))
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
		if (ImGui::DragFloat("##Y", &values.y, 0.01f, 0.0f, 0.0f, GetPrecision(values.x)))
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
		if (ImGui::DragFloat("##Z", &values.z, 0.01f, 0.0f, 0.0f, GetPrecision(values.x)))
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
			strncpy_s(buffer, sizeof(buffer), tag.c_str(), sizeof(buffer));
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

			if (!mContext.HasComponent<BoxColliderComponent>())
			{
				if (ImGui::MenuItem("Box Collider"))
				{
					mContext.AddComponent<BoxColliderComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::Separator();

			if (!mContext.HasComponent<UIPanelComponent>())
			{
				if (ImGui::MenuItem("UI Panel"))
				{
					mContext.AddComponent<UIPanelComponent>(CreateRef<UIPanel>());
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<UITextComponent>())
			{
				if (ImGui::MenuItem("UI Text"))
				{
					mContext.AddComponent<UITextComponent>(CreateRef<UIText>());
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<UIButtonComponent>())
			{
				if (ImGui::MenuItem("UI Button"))
				{
					mContext.AddComponent<UIButtonComponent>(CreateRef<UIButton>());
					ImGui::CloseCurrentPopup();
				}
			}

			bool noPlanetSpecific = mContext.HasComponent<TerrainColliderComponent>() && mContext.HasComponent<TerrainDetailComponent>() && mContext.HasComponent<TerrainObjectComponent>();

			if (mContext.HasComponent<PlanetComponent>() && !noPlanetSpecific)
			{
				ImGui::Separator();

				if (ImGui::BeginMenu("Planet Specific"))
				{
					if (!mContext.HasComponent<TerrainColliderComponent>())
					{
						if (ImGui::MenuItem("Terrain Collider"))
						{
							mContext.AddComponent<TerrainColliderComponent>();
							ImGui::CloseCurrentPopup();
						}
					}

					if (!mContext.HasComponent<TerrainDetailComponent>())
					{
						if (ImGui::MenuItem("Terrain Details")) 
						{
							mContext.AddComponent<TerrainDetailComponent>();
							ImGui::CloseCurrentPopup();
						}
					}

					if (!mContext.HasComponent<TerrainObjectComponent>())
					{
						if (ImGui::MenuItem("Terrain Objects"))
						{
							mContext.AddComponent<TerrainObjectComponent>();
							ImGui::CloseCurrentPopup();
						}
					}	

					ImGui::EndMenu();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		ImGui::TextDisabled("UUID: %llu", entity.GetComponent<IDComponent>().ID);

		DrawComponent<TransformComponent>(ICON_TOASTER_ARROWS_ALT" Transform", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				float fov = 45.0f;
				DirectX::XMMATRIX cameraTransform;

				bool entity2D = entity.HasComponent<UIPanelComponent>() || entity.HasComponent<UITextComponent>() || entity.HasComponent<UIButtonComponent>();

				bool updateTransform = false;
				bool updateRotTransform = false;

				auto [width, height] = Renderer::GetGPassPositionRT()->GetSize();

				if (entity2D)
				{
					DirectX::XMFLOAT3 translation2D = component.Translation;

					translation2D.x += width / 2.0f;
					translation2D.y += (height / 2.0f);

					updateTransform |= DrawFloat3Control("Translation", translation2D);

					translation2D.x -= width / 2.0f;
					translation2D.y -= (height / 2.0f);

					component.Translation = translation2D;

				}
				else
					updateTransform |= DrawFloat3Control("Translation", component.Translation);

				updateRotTransform |= DrawFloat3Control("Rotation", component.RotationEulerAngles);
				updateTransform |= DrawFloat3Control("Scale", component.Scale, 1.0f);

				if (updateRotTransform && entity.HasComponent<BoxColliderComponent>())
				{
					auto bcc = entity.GetComponent<BoxColliderComponent>();

					DirectX::XMVECTOR totalRotVec = DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&component.RotationQuaternion), DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(component.RotationEulerAngles.x), DirectX::XMConvertToRadians(component.RotationEulerAngles.y), DirectX::XMConvertToRadians(component.RotationEulerAngles.z)));
					DirectX::XMFLOAT4 totalRot;
					DirectX::XMStoreFloat4(&totalRot, totalRotVec);
				}

				component.IsDirty = updateTransform || updateRotTransform;
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
				if (!component.MeshObject->GetFilePath().empty())
					ImGui::InputText("##meshfilepath", (char*)component.MeshObject->GetFilePath().c_str(), 256, ImGuiInputTextFlags_ReadOnly);
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

						component.MeshObject = CreateRef<Mesh>(*filepath);
					}
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				if (component.MeshObject->HasLODGroups())
				{
					ImGui::Text("LOD Groups ");
					ImGui::TableSetColumnIndex(1);

					std::vector<float>& thresholds = component.MeshObject->GetLODThresholds();

					if (thresholds[0] > thresholds[1]) {
						std::swap(thresholds[0], thresholds[1]);
					}

					// Define bar dimensions
					ImVec2 region = ImGui::GetContentRegionAvail();
					float barHeight = 40.0f; // Adjust as needed
					float barWidth = region.x;

					// Reserve space for the LOD bar
					ImGui::Dummy(ImVec2(barWidth, barHeight));
					ImVec2 startPos = ImGui::GetItemRectMin();
					ImVec2 endPos = ImGui::GetItemRectMax();

					// Calculate actual bar width
					barWidth = endPos.x - startPos.x;

					ImDrawList* drawList = ImGui::GetWindowDrawList();

					// Draw bar background
					ImU32 bgColor = IM_COL32(60, 60, 60, 255);
					drawList->AddRectFilled(startPos, endPos, bgColor);

					// Calculate positions based on normalized thresholds
					float x_threshold0 = ImLerp(startPos.x, endPos.x, thresholds[0]);
					float x_threshold1 = ImLerp(startPos.x, endPos.x, thresholds[1]);

					// Draw LOD segments with distinct colors
					// LOD0: [0.0, threshold0] (Green)
					// LOD1: (threshold0, threshold1] (Yellow)
					// LOD2: (threshold1, 1.0] (Red)
					drawList->AddRectFilled(ImVec2(startPos.x, startPos.y), ImVec2(x_threshold0, endPos.y), IM_COL32(0, 255, 0, 255)); // Green
					drawList->AddRectFilled(ImVec2(x_threshold0, startPos.y), ImVec2(x_threshold1, endPos.y), IM_COL32(255, 255, 0, 255)); // Yellow
					drawList->AddRectFilled(ImVec2(x_threshold1, startPos.y), ImVec2(endPos.x, endPos.y), IM_COL32(255, 0, 0, 255)); // Red

					// Draw drag able handles for each threshold
					for (int i = 0; i < 2; ++i) {
						ImGui::PushID(i);
						float threshold = thresholds[i];
						float handleX = ImLerp(startPos.x, endPos.x, threshold);

						// Draw a vertical line at the threshold
						ImU32 lineColor = IM_COL32(200, 200, 200, 255);
						drawList->AddLine(ImVec2(handleX, startPos.y), ImVec2(handleX, endPos.y), lineColor, 2.0f);

						// Define handle dimensions
						float handleHalfWidth = 8.0f; // Clickable area width
						ImRect handleRect(ImVec2(handleX - handleHalfWidth, startPos.y),
							ImVec2(handleX + handleHalfWidth, endPos.y));

						// Create an invisible button for the handle with a unique label
						std::string handleLabel = "handle" + std::to_string(i);
						ImGui::SetCursorScreenPos(handleRect.Min);
						bool hovered = ImGui::InvisibleButton(handleLabel.c_str(), ImVec2(handleRect.GetWidth(), handleRect.GetHeight()));

						// Show tooltip with current threshold value on hover
						if (ImGui::IsItemHovered()) {
							ImGui::BeginTooltip();
							ImGui::Text("LOD%d Threshold: %.2f", i, thresholds[i]);
							ImGui::EndTooltip();
						}

						// Highlight handle if hovered or active
						if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
							ImU32 highlightColor = IM_COL32(255, 255, 255, 100);
							drawList->AddRectFilled(handleRect.Min, handleRect.Max, highlightColor);
						}

						// Handle dragging of the threshold
						if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
							float mouseX = ImGui::GetIO().MousePos.x;

							// Compute new normalized position based on mouse X
							float new_norm = (mouseX - startPos.x) / barWidth;
							new_norm = std::clamp(new_norm, 0.0f, 1.0f);
							float new_threshold = new_norm;

							// Maintain ordering: thresholds[0] < thresholds[1]
							if (i == 0) { // LOD0 threshold (Left)
								float epsilon = 0.01f; // Minimal separation
								new_threshold = (std::min)(new_threshold, thresholds[1] - epsilon);
							}
							else { // LOD1 threshold (Right)
								float epsilon = 0.01f;
								new_threshold = (std::max)(new_threshold, thresholds[0] + epsilon);
							}

							// Clamp to [0.0, 1.0]
							new_threshold = std::clamp(new_threshold, 0.0f, 1.0f);

							// Update the threshold
							thresholds[i] = new_threshold;

							// Ensure thresholds are sorted
							if (thresholds[0] > thresholds[1]) {
								std::swap(thresholds[0], thresholds[1]);
							}

							// Persist the updated thresholds
							component.MeshObject->SetLODThresholds(thresholds);
						}

						ImGui::PopID();
					}
				}

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
				{
					camera.SetPerspectiveVerticalFOV(perspectiveVerticalFOV);
					component.IsDirty = true;
				}

				float n = camera.GetNearClip();
				if (DrawFloatControl("Near Clip", n, 90.0f))
				{
					camera.SetNearClip(n);
					component.IsDirty = true;
				}

				float f = camera.GetFarClip();
				if (DrawFloatControl("Far Clip", f, 90.0f, 0.0f, 0.0f, 10.0f)) 
				{
					camera.SetFarClip(f);
					component.IsDirty = true;
				}

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
				ImGui::Text("Subdivisions");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::SliderInt("##Subdivisions", &subdivions, 0, 20))
					component.Subdivisions = subdivions;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Max Alt(km)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##MaxAltitude", &component.PlanetData.maxAltitude, 0.1f, 0.0f, 0.0f, "%.2f")) 
				{
					if (entity.HasComponent<TerrainColliderComponent>()) 
					{
						entity.GetComponent<TerrainColliderComponent>().Collider->mMaxAltitude = component.PlanetData.maxAltitude + component.PlanetData.radius;
						entity.GetComponent<TerrainColliderComponent>().Collider->CalculateBounds();
					}

					modified = true;
				}

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
				{
					PlanetSystem::CalculateBasePlanet(component, component.PlanetData.radius);

					if (entity.HasComponent<TerrainColliderComponent>())
					{
						entity.GetComponent<TerrainColliderComponent>().Collider->mMaxAltitude = component.PlanetData.maxAltitude + component.PlanetData.radius;
						entity.GetComponent<TerrainColliderComponent>().Collider->CalculateBounds();
					}

					modified = true;
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Gravitational acceleration(m/s^2)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##GravAcc", &component.PlanetData.gravAcc, 0.1f, 0.0f, 0.0f, "%.2f"))
					modified = true;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Smooth Shading)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::Checkbox("##smoothShading", &component.PlanetData.smoothShading);

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
					ImGui::DragFloat("##rayScaleHeight", &component.PlanetData.rayScaleHeight, 10.0f, 0.0f, 15000.0f, "%.0f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Mie\nScale Height");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##mieScaleHeight", &component.PlanetData.mieScaleHeight, 10.0f, 0.0f, 5000.0f, "%.0f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ray Scattering\nCoefficient Red");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##rayScatteringnCoefficientRed", &component.PlanetData.rayBaseScatteringCoefficient.x, 0.000001f, 0.0f, 1.0f, "%.7f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ray Scattering\nCoefficient Green");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##rayScatteringnCoefficientGreen", &component.PlanetData.rayBaseScatteringCoefficient.y, 0.000001f, 0.0f, 1.0f, "%.7f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ray Scattering\nCoefficient Blue");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##rayScatteringnCoefficientBlue", &component.PlanetData.rayBaseScatteringCoefficient.z, 0.000001f, 0.0f, 1.0f, "%.7f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Mie Scattering");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##mieScattering", &component.PlanetData.mieBaseScatteringCoefficient, 0.0001f, 0.0f, 1.0f, "%.4f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sun Disc");
					ImGui::TableSetColumnIndex(1);
					ImGui::Checkbox("##checkbox", &component.PlanetData.SunDisc);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sun Disc Radius");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##sundiscradius", &component.PlanetData.SunDiscRadius, 0.0001f, 0.0f, 2*M_PI, "%.5f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sun Glow Intensity");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##sunglowintensity", &component.PlanetData.SunGlowIntensity, 0.1f, 0.0f, 10.0f, "%.2f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sun Edge Softness");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##sunedgesoftness", &component.PlanetData.SunEdgeSoftness, 0.0001f, 0.0f, 2 * M_PI, "%.5f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sun Glow Sizes");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##sunglowsize", &component.PlanetData.SunGlowSize, 0.01f, 0.0f, 2 * M_PI, "%.3f");
				}

				ImGui::EndTable();

				if(modified)
					component.IsDirty = true;
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
				ImGui::Text("Sun Desired Coverage Area");
				ImGui::TableSetColumnIndex(1);
				ImGui::DragFloat("##sundesiredcoverage", &component.SunDesiredCoverage, 1.0f, 0.0f, 20000.0f, "%.1f");

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Sun Light Distance");
				ImGui::TableSetColumnIndex(1);
				ImGui::DragFloat("##sunlightdistance", &component.SunLightDistance, 1.0f, 0.0f, 10000.0f, "%.1f");

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

		DrawComponent<ScriptComponent>(ICON_TOASTER_CODE" Script", entity, mScene, [=](auto& component, Entity entity, Scene* scene)
			{
				bool scriptClassExists = ScriptEngine::EntityClassExists(component.ClassName);
				
				static char buffer[64];
				strcpy_s(buffer, sizeof(buffer), component.ClassName.c_str());

				if (!scriptClassExists)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.3f, 1.0f));

				if (ImGui::InputText("Class", buffer, sizeof(buffer)))
				{
					component.ClassName = buffer;
					bool validScriptClass = ScriptEngine::EntityClassExists(component.ClassName);
				}

				// Fields

				// If Scene running
				if (scene->mIsRunning)
				{
					Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
					if (scriptInstance)
					{
						const auto& fields = scriptInstance->GetScriptClass()->GetFields();

						for (const auto& [name, field] : fields)
						{
							if (field.Type == ScriptFieldType::Float)
							{
								float data = scriptInstance->GetFieldValue<float>(name);
								if (ImGui::DragFloat(name.c_str(), &data))
								{
									scriptInstance->SetFieldValue<float>(name, data);
								}
							}
						}
					}
				}
				else
				{
					if (scriptClassExists)
					{
						Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(component.ClassName);
						const auto& fields = entityClass->GetFields();

						auto& entityFields = ScriptEngine::GetScriptFieldMap(entity);
						for (const auto& [name, field] : fields)
						{
							// Field has been set in the editor
							if (entityFields.find(name) != entityFields.end())
							{
								ScriptFieldInstance& scriptField = entityFields.at(name);

								if (field.Type == ScriptFieldType::Float)
								{
									float data = scriptField.GetValue<float>();
									if (ImGui::DragFloat(name.c_str(), &data))
										scriptField.SetValue(data);
								}
							}
							else
							{
								// Display control to set it maybe
								if (field.Type == ScriptFieldType::Float)
								{
									float data = 0.0f;
									if (ImGui::DragFloat(name.c_str(), &data))
									{
										ScriptFieldInstance& fieldInstance = entityFields[name];
										fieldInstance.Field = field;
										fieldInstance.SetValue(data);
									}
								}
							}
						}
					}
				}

				if (!scriptClassExists)
					ImGui::PopStyleColor();
			});

		DrawComponent<RigidBodyComponent>(ICON_TOASTER_HAND_ROCK_O" Rigid Body", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				float temp;

				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("RigidBody", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Center of Mass");
				ImGui::TableSetColumnIndex(1);
				DrawDouble3Control("CenterOfMass", component.CenterOfMass);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Mass (kg)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				float mass = 1.0f / (float)component.InvMass;
				ImGui::DragFloat("##label", &mass, 0.1f, 0.0f, 60000.0f, "%.1f");
				component.InvMass = 1.0f / mass;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Elasticity (0-1)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				temp = static_cast<float>(component.Elasticity);
				if(ImGui::DragFloat("##elasticity", &temp, 0.01f, 0.0f, 1.0f, "%.01f"))
					component.Elasticity = static_cast<double>(temp);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Friction (0-1)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				temp = static_cast<float>(component.Friction);
				if(ImGui::DragFloat("##friction", &temp, 0.01f, 0.0f, 1.0f, "%.01f"))
					component.Friction = static_cast<double>(temp);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Linear Damping (0-1)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				temp = static_cast<float>(component.LinearDamping);
				if (ImGui::DragFloat("##lineardamping", &temp, 0.01f, 0.0f, 25.0f, "%.1f"))
					component.LinearDamping = static_cast<double>(temp);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Angular Damping (0-1)");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				temp = static_cast<float>(component.AngularDamping);
				if (ImGui::DragFloat("##angulardamping", &temp, 0.01f, 0.0f, 10.0f, "%.2f"))
					component.AngularDamping = static_cast<double>(temp);

				ImGui::EndTable();
			});

		DrawComponent<SphereColliderComponent>(ICON_TOASTER_CIRCLE_O" Sphere Collider", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				float temp;

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
				temp = static_cast<float>(component.Collider->mRadius);
				if (ImGui::DragFloat("##radius", &temp, 0.1f, 0.0f, 600.0f, "%.4f")) 
				{
					component.Collider->mRadius = static_cast<double>(temp);

					component.Collider->CalculateBounds();
					component.Collider->CalculateInertiaTensor();
				}
				ImGui::EndTable();
			});

		DrawComponent<BoxColliderComponent>(ICON_TOASTER_CUBE" Box Collider", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::Checkbox("Render Collider", &component.RenderCollider);

				ImGui::BeginTable("BoxCollider", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Size");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (DrawDouble3Control("Size", component.Collider->mSize, 1.0f)) 
				{
					component.Collider->CalculateBounds();
					component.Collider->CalculateInertiaTensor();
				}
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
				if (!component.Collider->mFilePath.empty())
					ImGui::InputText("##heightmapfilepath", (char*)component.Collider->mFilePath.c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##heightmapfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openheightmapfilepath")) 
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.png", "..\\Toaster\\assets\\textures\\");

					if (filepath) 
					{
						PlanetComponent& pc = entity.GetComponent<PlanetComponent>();

						component.Collider->mFilePath = *filepath;
						PhysicsEngine::LoadTerrainData(component.Collider->mFilePath.c_str(), pc.PlanetData.maxAltitude, pc.PlanetData.minAltitude);
					}
				}

				ImGui::EndTable();
			});

		DrawComponent<UIPanelComponent>(ICON_TOASTER_SQUARE_O" UI Panel", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("##panelTable", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);

				std::optional<std::string> textureFilepathOpt = component.Panel->GetTextureFilepath();
				const std::string& textureFilepath = (textureFilepathOpt && !textureFilepathOpt->empty()) ?
					*textureFilepathOpt :
					"assets/textures/Checkerboard.png";

				void* textureID = (void*)(uintptr_t)TextureLibrary::Get(textureFilepath)->GetID();

				ImGui::Image(textureID, ImVec2(64.0f, 64.0f));

				std::optional<std::string> filepath;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = std::filesystem::path(gAssetPath) / path;
						filepath = completePath.string();

						if (filepath)
						{
							component.Panel->SetTextureFilepath(*filepath);
							TextureLibrary::LoadTexture2D(*filepath);
						}
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					filepath = FileDialogs::OpenFile("", "..\\Toaster\\assets\\textures\\");

					if (filepath)
					{
						component.Panel->SetTextureFilepath(*filepath);
						TextureLibrary::LoadTexture2D(*filepath);
					}
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::BeginTable("##table2", 2, flags);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				bool useColor = component.Panel->GetUseColor();
				if (ImGui::Checkbox("Use##Color", &useColor))
				{
					component.Panel->SetUseColor(useColor);
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##color", component.Panel->GetColor());
				ImGui::EndTable();
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Corner Radius");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##cornerradius", component.Panel->GetCornerRadius(), 0.0f, 50.0f, "%.1f");
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Border size");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##bordersize", component.Panel->GetBorderSize(), 0.0f, 50.0f, "%.1f");

				ImGui::EndTable();

			/*	ImGui::BeginTable("UIPanelComponentTable", 3, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Texture ");
				ImGui::TableSetColumnIndex(1);
				if (!component.Panel->GetFilePath().empty())
					ImGui::InputText("##uitexturefilepath", (char*)component.Panel->GetFilePath().c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##uitexturefilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openuitexturefilepath"))
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.png", "..\\Toaster\\assets\\textures\\");

					if (filepath)
						component.Panel->SetFilePath(*filepath);
				}
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##color", component.Panel->GetColor());

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Corner Radius");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##cornerradius", component.Panel->GetCornerRadius(), 0.0f, 50.0f, "%.1f");

				ImGui::EndTable();*/
			});

		DrawComponent<UITextComponent>(ICON_TOASTER_FILE_TEXT" UI Text", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				auto& text = component.Text->GetText();

				char buffer[1024 * 5];
				memset(buffer, 0, sizeof(buffer));
				strncpy_s(buffer, text.c_str(), sizeof(buffer));
				ImGuiTableFlags textFlags = ImGuiInputTextFlags_CtrlEnterForNewLine;
				if (ImGui::InputTextMultiline("##text", buffer, IM_ARRAYSIZE(buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), textFlags)) 
				{
					text = std::string(buffer);
					component.Text->SetText(text);
				}
					

				ImGui::BeginTable("##FontTable", 3, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Font ");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (!component.Text->GetFont()->GetFilePath().empty())
					ImGui::InputText("##fontfilepath", (char*)component.Text->GetFont()->GetFilePath().c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##fontfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openfont"))
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.ttf", "..\\Toaster\\assets\\fonts\\");
					if (filepath) 
						component.Text->SetFont(CreateRef<Font>(*filepath));	
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##color", component.Text->GetColor());
				ImGui::TableSetColumnIndex(0);

				ImGui::PopItemWidth();

				ImGui::EndTable();
			});

		DrawComponent<UIButtonComponent>(ICON_TOASTER_SQUARE_O" UI Button", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("UIButtonComponent", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##buttoncolor", component.Button->GetColor());

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("ClickColor");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##clickcolor", component.Button->GetClickColor());

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Corner Radius");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##cornerradius", component.Button->GetCornerRadius(), 0.0f, 50.0f, "%.1f");

				ImGui::EndTable();
			});

		DrawComponent<TerrainDetailComponent>(ICON_TOASTER_GLOBE" Terrain Detail", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("##TerrainDetailComponent", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Seed");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::InputText("##seed", (char*)std::to_string(component.Seed).c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::PopStyleColor();
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Subdivision");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderInt("##subdivisionactivation", &component.SubdivisionActivation, 0, 20);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Octaves");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderInt("##octaves", &component.Octaves, 0, 10);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Frequency");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##frequency", &component.Frequency, 0.1, 64.0f, "%.1f");
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Amplitude");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##amplitude", &component.Amplitude, 0.1, 256.0f, "%.1f");
				ImGui::EndTable();
			});

		DrawComponent<TerrainObjectComponent>(ICON_TOASTER_CUBE" Terrain Object", entity, mScene, [](auto& component, Entity entity, Scene* scene)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("##TerrainObjectComponent", 3, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.6156f);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Mesh ");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (component.MeshObject)
				{
					if (!component.MeshObject->GetFilePath().empty())
						ImGui::InputText("##terrainobjectfilepath", (char*)component.MeshObject->GetFilePath().c_str(), 256, ImGuiInputTextFlags_ReadOnly);
					else
						ImGui::InputText("##terrainobjectfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				}
				else
					ImGui::InputText("##terrainobjectfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openmesh"))
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.gltf", "..\\Toaster\\assets\\meshes\\");
					if (filepath) 
					{
						component.MeshObject = CreateRef<Mesh>(*filepath, DirectX::XMFLOAT3(0.0, 0.0, 0.0), true, component.MaxNrOfObjects);
					}
				}
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Subdivision");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderInt("##subdivisionactivation", &component.SubdivisionActivation, 0, 20);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Max per face");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderInt("##maxnrofobjectsperface", &component.MaxNrOfObjectPerFace, 0, 20);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Total max");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderInt("##maxnrofobjects", &component.MaxNrOfObjects, 0, 1000);
				ImGui::EndTable();

			});
	}

}