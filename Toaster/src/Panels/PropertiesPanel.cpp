#include "PropertiesPanel.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Toast/ImGui/ImGuiHelpers.h"

#include "Toast/Core/UUID.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"
#include "Toast/Physics/Bounds.h"

#include "Toast/Scene/Components.h"

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

	static uint32_t sCounter = 0;
	static char sIDBuffer[16];

	static const std::string& PrettifyScriptFieldName(const std::string& name)
	{
		static std::unordered_map<std::string, std::string> sPrettyNameCache;

		auto it = sPrettyNameCache.find(name);
		if (it != sPrettyNameCache.end())
			return it->second;

		std::string result;
		result.reserve(name.size() + 8);

		for (size_t i = 0; i < name.size(); i++)
		{
			const unsigned char c = (unsigned char)name[i];

			if (c == '_')
			{
				if(!result.empty() && result.back() != ' ')
					result += ' ';
				continue;
			}

			// Only look for a boundary if we didn't just emit a space, otherwise
			// "m_Speed" would end up as "m  Speed".
			if (i > 0 && !result.empty() && result.back() != ' ')
			{
				const unsigned char prev = (unsigned char)name[i - 1];

				// lower/digit -> upper: the normal camelCase word start
				const bool startOfWord = std::isupper(c) && !std::isupper(prev);

				// upper -> upper followed by lower: the last letter of an acronym is
				// actually the first letter of the next word ("GPUTime" -> "GPU Time")
				const bool endOfAcronym = std::isupper(c) && std::isupper(prev)
					&& (i + 1) < name.size() && std::islower((unsigned char)name[i + 1]);

				// letter -> digit, so "Engine2" reads as "Engine 2"
				const bool startOfNumber = std::isdigit(c) && !std::isdigit(prev);

				if (startOfWord || endOfAcronym || startOfNumber)
					result += ' ';
			}

			result += (char)c;
		}

		return sPrettyNameCache.emplace(name, std::move(result)).first->second;
	}

	static bool DrawFloatControl(const std::string& label, float& value, WindowsWindow* window, std::string& activeDragArea, float imGuiTableWidth = 90.0f, float min = 0.0f, float max = 0.0f, float delta = 0.5f, const char* displayFormat = "%.1f"){
		bool modified = false;
		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		// We will measure how tall the label text will be when wrapped at `imGuiTableWidth`.
		// ImGui::CalcTextSize can do wrapping if we pass a 'wrap_width' parameter.
		float wrapWidth = imGuiTableWidth - ImGui::GetStyle().ItemSpacing.x;
		if (wrapWidth < 1.0f)
			wrapWidth = 1.0f;

		// Temporarily set a wrap pos so CalcTextSize accounts for wrapping
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
		ImVec2 textSize = ImGui::CalcTextSize(label.c_str(), nullptr, false, wrapWidth);
		ImGui::PopTextWrapPos();

		// The drag widget typically has about one line of height:
		float dragLineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;

		// The row must be at least as tall as our text or the drag area, whichever is bigger:
		float rowHeight = textSize.y;
		if (rowHeight < dragLineHeight)
			rowHeight = dragLineHeight;

		ImGui::PushID(label.c_str());
		if (ImGui::BeginTable("##table2", 2, flags))
		{
			// Fix the first column to imGuiTableWidth, second column is the remainder
			ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, imGuiTableWidth);
			ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - imGuiTableWidth);

			// Enforce a specific height for this row
			ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

			// --- Column 0: The label text ---
			ImGui::TableSetColumnIndex(0);
			{
				// Vertical offset so the text is centered if the row is taller than the text
				float offsetY = (rowHeight - textSize.y) * 0.5f;
				if (offsetY < 0.0f)
					offsetY = 0.0f;

				// Move the cursor down by offsetY
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

				// Wrap the text at the end of this column
				ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
				ImGui::TextUnformatted(label.c_str());
				ImGui::PopTextWrapPos();
			}

			ImGui::TableSetColumnIndex(1);
			{
				float offsetY = (rowHeight - dragLineHeight) * 0.5f;
				if (offsetY < 0.0f)
					offsetY = 0.0f;

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
				ImGui::PushItemWidth(-1);

				// This is the same drag area size you used before
				ImVec2 dragAreaSize(contentRegionAvailable.x - imGuiTableWidth,	dragLineHeight);
				if (ImGuiHelpers::ManualDragFloat(label.c_str(), value, window, activeDragArea, delta, dragAreaSize, displayFormat, min, max))
					modified = true;

				ImGui::PopItemWidth();
			}

			ImGui::EndTable();
		}
		ImGui::PopID();

		return modified;
	}

	static std::string SanitizeNamespace(const std::string& name)
	{
		std::string result;
		result.reserve(name.size());

		for (char c : name)
			if (std::isalnum((unsigned char)c) || c == '_')
				result += c;

		if (result.empty())
			return "Project";                       // fallback for a name with nothing usable

		if (std::isdigit((unsigned char)result[0]))
			result.insert(result.begin(), '_');     // identifiers can't start with a digit

		return result;
	}

	PropertiesPanel::PropertiesPanel(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel, WindowsWindow* window)
	{
		SetContext(context, sceneHierarchyPanel, window);
	}

	void PropertiesPanel::SetContext(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel, WindowsWindow* window)
	{
		mSceneHierarchyPanel = sceneHierarchyPanel;
		mScene = mSceneHierarchyPanel->GetContext();

		mContext = context;

		mWindow = window;
	}

	void PropertiesPanel::SetProjectPath(const std::filesystem::path& projectPath, const std::string& projectName)
	{
		std::filesystem::path cleanPath = projectPath;
		if (cleanPath.has_filename() == false)
			cleanPath = cleanPath.parent_path();

		mAssetRoot = cleanPath / "Assets";
		mProjectName = projectName;
	}

	void PropertiesPanel::RequestTextureImport(const std::filesystem::path& path, bool defaultSRGB, std::function<void(AssetHandle)> onComplete)
	{
		// Check if this file is already registered — no need for the popup
		auto assetDir = AssetManager::GetAssetDirectory();
		auto relativePath = std::filesystem::relative(path, assetDir);
		AssetHandle existing = AssetManager::GetHandleFromPath(relativePath);

		if (existing != AssetHandle(0))
		{
			if (onComplete)
				onComplete(existing);
			return;
		}

		mPendingImportPath = path;
		mPendingExportPath = "Textures";
		mPendingImportSRGB = defaultSRGB;
		mOnImportComplete = onComplete;
		mPendingImportOpen = true;
	}

	void PropertiesPanel::RequestUITextureImport(const std::filesystem::path& path, bool defaultSRGB, std::function<void(AssetHandle)> onComplete)
	{
		// Check if this file is already registered — no need for the popup
		auto assetDir = AssetManager::GetAssetDirectory();
		auto relativePath = std::filesystem::relative(path, assetDir);
		AssetHandle existing = AssetManager::GetHandleFromPath(relativePath);

		if (existing != AssetHandle(0))
		{
			if (onComplete)
				onComplete(existing);
			return;
		}

		mPendingImportPath = path;
		mPendingExportPath = "Textures\\UI";
		mPendingImportSRGB = defaultSRGB;
		mOnImportComplete = onComplete;
		mPendingImportOpen = true;
	}

	void PropertiesPanel::DrawImportTexturePopup()
	{
		if (!mPendingImportOpen)
			return;

		ImGui::OpenPopup("Import Texture Settings");

		if (ImGui::BeginPopupModal("Import Texture Settings", &mPendingImportOpen,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Importing: %s", mPendingImportPath.filename().string().c_str());
			ImGui::Checkbox("sRGB (color data)", &mPendingImportSRGB);

			if (ImGui::Button("Import"))
			{
				AssetHandle handle = AssetManager::ImportExternalAsset(mPendingImportPath, mPendingExportPath);

				AssetEntry* entry = AssetManager::GetEntry(handle);
				if (entry)
					entry->Texture2DSettings.ForceSRGB = mPendingImportSRGB;

				if (mOnImportComplete)
					mOnImportComplete(handle);

				mPendingImportOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				mPendingImportOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void PropertiesPanel::OnImGuiRender(std::string& activeDragArea)
	{
		ImGui::Begin(ICON_TOASTER_WRENCH" Properties");

		mContext = mSceneHierarchyPanel->GetSelectedEntity();
		mScene = mSceneHierarchyPanel->GetContext();

		if (mContext)
			DrawComponents(mContext, activeDragArea);

		ImGui::End();

		DrawImportTexturePopup();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, Scene* scene, std::string& activeDragArea, WindowsWindow* window, std::filesystem::path& assetRoot, UIFunction uiFunction)
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
				uiFunction(component, entity, scene, window, activeDragArea, assetRoot);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponents<T>();
		}
	}

	void PropertiesPanel::DrawComponents(Entity entity, std::string& activeDragArea)
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
					mContext.AddComponent<MeshComponent>();
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

			if (!mContext.HasComponent<ScriptComponent>() && !mContext.HasComponent<SceneScriptComponent>())
			{
				if (ImGui::MenuItem("Script"))
				{
					mContext.AddComponent<ScriptComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<ScriptComponent>() && !mContext.HasComponent<SceneScriptComponent>())
			{
				if (ImGui::MenuItem("Scene Script"))
				{
					mContext.AddComponent<SceneScriptComponent>();
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

			if (!mContext.HasComponent<ParticlesComponent>())
			{
				if (ImGui::MenuItem("Particles"))
				{
					mContext.AddComponent<ParticlesComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<MoveableComponent>())
			{
				if (ImGui::MenuItem("Moveable"))
				{
					mContext.AddComponent<MoveableComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::Separator();

			if (!mContext.HasComponent<UIPanelComponent>())
			{
				if (ImGui::MenuItem("UI Panel"))
				{
					mContext.AddComponent<UIPanelComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<UITextComponent>())
			{
				if (ImGui::MenuItem("UI Text"))
				{
					mContext.AddComponent<UITextComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!mContext.HasComponent<UIButtonComponent>())
			{
				if (ImGui::MenuItem("UI Button"))
				{
					mContext.AddComponent<UIButtonComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		ImGui::TextDisabled("UUID: %llu", entity.GetComponent<IDComponent>().ID);

		DrawComponent<TransformComponent>(ICON_TOASTER_ARROWS_ALT" Transform", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				float fov = 45.0f;
				DirectX::XMMATRIX cameraTransform;

				bool entity2D = entity.HasComponent<UIPanelComponent>() || entity.HasComponent<UITextComponent>() || entity.HasComponent<UIButtonComponent>();

				bool updateTransform = false;
				bool updateRotTransform = false;

				auto [width, height] = Renderer::GetGPassPositionRT()->GetSize();

				if (entity2D)
				{
					DirectX::XMFLOAT3 translation2D = component.Translation;

					translation2D.x += (width / 2.0f);
					translation2D.y += (height / 2.0f);

					updateTransform |= ImGuiHelpers::ManualDragFloat3("Translation", translation2D, 1.0f, 0.0f, window, activeDragArea);

					translation2D.x -= (width / 2.0f);
					translation2D.y -= (height / 2.0f);

					component.Translation = translation2D;
				}
				else
					updateTransform |= ImGuiHelpers::ManualDragFloat3("Translation", component.Translation, 1.0f, 0.0f, window, activeDragArea);

				updateRotTransform |= ImGuiHelpers::ManualDragFloat3("Rotation", component.RotationEulerAngles, 0.1f, 0.0f, window, activeDragArea);

				updateTransform |= ImGuiHelpers::ManualDragFloat3("Scale", component.Scale, 1.0f, 0.0f, window, activeDragArea);

				if (updateRotTransform && entity.HasComponent<BoxColliderComponent>())
				{
					auto bcc = entity.GetComponent<BoxColliderComponent>();

					DirectX::XMVECTOR totalRotVec = DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&component.RotationQuaternion), DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(component.RotationEulerAngles.x), DirectX::XMConvertToRadians(component.RotationEulerAngles.y), DirectX::XMConvertToRadians(component.RotationEulerAngles.z)));
					DirectX::XMFLOAT4 totalRot;
					DirectX::XMStoreFloat4(&totalRot, totalRotVec);
				}

				component.IsDirty = updateTransform || updateRotTransform;
			});

		DrawComponent<MeshComponent>(ICON_TOASTER_CUBE" Mesh", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
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

				const AssetMetadata* meta = AssetManager::GetMetadata(component.MeshHandle);
				std::string label = meta ? meta->FilePath.filename().string() : "Empty";

				ImGui::InputText("##meshfilepath", label.data(), label.size() + 1, ImGuiInputTextFlags_ReadOnly);

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

						// Parts exists, erase them first
						for (UUID partUUID : component.PartEntities)
						{
							if (partUUID == 0)
								continue;

							Entity child = scene->FindEntityByUUID(partUUID);
							if (!child)
								continue;

							entity.RemoveChild(child);
							scene->DestroyEntity(child);
						}
						component.PartEntities.clear();
						component.MeshHandle = AssetManager::ImportExternalAsset(*filepath, "Meshes");

						if (AssetManager::IsHandleValid(component.MeshHandle))
							scene->AddMeshPartEntities(component, entity);
					}
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(component.MeshHandle);

				if (mesh && mesh->HasLODGroups())
				{
					ImGui::Text("LOD Groups ");
					ImGui::TableSetColumnIndex(1);

					std::vector<float>& thresholds = mesh->GetLODThresholds();

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

					ImU32 color_LOD0 = IM_COL32(0, 120, 0, 200); // Light Green with some transparency
					ImU32 color_LOD1 = IM_COL32(255, 216, 0, 200); // Light Yellow with some transparency
					ImU32 color_LOD2 = IM_COL32(120, 0, 0, 200); // Light Pink with some transparency
					ImU32 barBackgroundColor = IM_COL32(50, 50, 50, 150); // Dark Gray with transparency
					ImU32 handleHighlightColor = IM_COL32(255, 255, 255, 150); // White with transparency

					// Draw bar background
					drawList->AddRectFilled(startPos, endPos, barBackgroundColor);

					// Calculate positions based on normalized thresholds
					float x_threshold0 = ImLerp(startPos.x, endPos.x, thresholds[0]);
					float x_threshold1 = ImLerp(startPos.x, endPos.x, thresholds[1]);

					// Draw LOD segments with distinct colors
					// LOD0: [0.0, threshold0] (Green)
					// LOD1: (threshold0, threshold1] (Yellow)
					// LOD2: (threshold1, 1.0] (Red)
					drawList->AddRectFilled(ImVec2(startPos.x, startPos.y), ImVec2(x_threshold0, endPos.y), color_LOD0); // Green
					drawList->AddRectFilled(ImVec2(x_threshold0, startPos.y), ImVec2(x_threshold1, endPos.y), color_LOD1); // Yellow
					drawList->AddRectFilled(ImVec2(x_threshold1, startPos.y), ImVec2(endPos.x, endPos.y), color_LOD2); // Red

					// Draw drag able handles for each threshold
					for (int i = 0; i < 2; ++i) {
						ImGui::PushID(i);
						float threshold = thresholds[i];
						float handleX = ImLerp(startPos.x, endPos.x, threshold);

						// Draw a vertical line at the threshold
						ImU32 lineColor = IM_COL32(200, 200, 200, 255);
						drawList->AddLine(ImVec2(handleX, startPos.y), ImVec2(handleX, endPos.y), lineColor, 1.0f);

						// Define handle dimensions
						float handleHalfWidth = 4.0f; // Clickable area width
						ImRect handleRect(ImVec2(handleX - handleHalfWidth, startPos.y), ImVec2(handleX + handleHalfWidth, endPos.y));

						// Create an invisible button for the handle with a unique label
						std::string handleLabel = "handle" + std::to_string(i);
						ImGui::SetCursorScreenPos(handleRect.Min);
						bool hovered = ImGui::InvisibleButton(handleLabel.c_str(), ImVec2(handleRect.GetWidth(), handleRect.GetHeight()));

						// Show tooltip with current threshold value on hover
						if (ImGui::IsItemHovered()) 
						{
							ImGui::BeginTooltip();
							ImGui::Text("LOD%d Threshold: %.2f", i, thresholds[i]);
							ImGui::EndTooltip();
						}

						// Highlight handle if hovered or active
						if (ImGui::IsItemHovered() || ImGui::IsItemActive()) 
							drawList->AddRectFilled(handleRect.Min, handleRect.Max, handleHighlightColor, 5.0f);

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
							mesh->SetLODThresholds(thresholds);
						}

						ImGui::PopID();
					}

					float lodDistance = component.LODDistance;

					// Calculate the x position based on normalized LOD distance
					float x_lod = ImLerp(startPos.x, endPos.x, lodDistance);

					// Calculate the y position to center the dot vertically on the bar
					float y_lod = (startPos.y + endPos.y) / 2.0f;

					// Define the dot's properties
					float dotRadius = 5.0f; // Adjust size as needed
					ImU32 dotColor = IM_COL32(0, 0, 0, 255); // Solid Black

					// Draw the filled circle (dot)
					drawList->AddCircleFilled(ImVec2(x_lod, y_lod), dotRadius, dotColor);

					// Optionally, add a border to the dot for better visibility against various bar colors
					ImU32 dotBorderColor = IM_COL32(255, 255, 255, 255); // Solid White Border
					drawList->AddCircle(ImVec2(x_lod, y_lod), dotRadius, dotBorderColor, 12, 2.0f); // 12 segments, 2.0f thickness

					// Define a unique identifier for the dot to handle hover detection
					std::string dotID = "LOD_Dot";

					// Create an invisible button over the dot's area for hover detection
					ImGui::SetCursorScreenPos(ImVec2(x_lod - dotRadius, y_lod - dotRadius));
					ImGui::InvisibleButton(dotID.c_str(), ImVec2(dotRadius * 2, dotRadius * 2));

					// Check if the invisible button (dot) is hovered
					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text("Current LOD Distance: %.2f", lodDistance);
						ImGui::EndTooltip();
					}
				}

				ImGui::EndTable();
			});

		DrawComponent<CameraComponent>(ICON_TOASTER_CAMERA" Camera", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
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
				if (DrawFloatControl("Vertical FOV", perspectiveVerticalFOV, window, activeDragArea, 90.0f))
				{
					camera.SetPerspectiveVerticalFOV(perspectiveVerticalFOV);
					component.IsDirty = true;
				}

				float n = camera.GetNearClip();
				if (DrawFloatControl("Near Clip", n, window, activeDragArea, 90.0f))
				{
					camera.SetNearClip(n);
					component.IsDirty = true;
				}

				float f = camera.GetFarClip();
				if (DrawFloatControl("Far Clip", f, window, activeDragArea, 90.0f, 0.0f, 0.0f, 10.0f))
				{
					camera.SetFarClip(f);
					component.IsDirty = true;
				}

				float orthoWidth = camera.GetOrthographicWidth();
				float orthoHeight = camera.GetOrthographicHeight();
				if (DrawFloatControl("Ortho Width", orthoWidth, window, activeDragArea, 90.0f) || DrawFloatControl("Ortho Height", orthoHeight, window, activeDragArea, 90.0f))
					camera.SetOrthographicSize(orthoWidth, orthoHeight);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("SpriteRendererTable", 2, flags);
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

		DrawComponent<DirectionalLightComponent>(ICON_TOASTER_SUN_O" Directional Light", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
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

				ImGui::EndTable();

				DrawFloatControl("Intensity", component.Intensity, window, activeDragArea, 90.0f, 0.0f, 25.0f, 0.01f, "%.2f");

				DrawFloatControl("Sun Desired Coverage Area", component.SunDesiredCoverage, window, activeDragArea, 90.0f, 0.0f, 20000.0f, 1.0f);

				DrawFloatControl("Sun Light Distance", component.SunLightDistance, window, activeDragArea, 90.0f, 0.0f, 10000, 1.0f);
			});

		DrawComponent<ScriptComponent>(ICON_TOASTER_CODE" Script", entity, mScene, activeDragArea, mWindow, mAssetRoot, [=](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{			
				static char buffer[64];
				strcpy_s(buffer, sizeof(buffer), component.ClassName.c_str());

				std::string scriptNamespace = SanitizeNamespace(mProjectName);

				bool validScriptClass = ScriptEngine::EntityClassExists(component.ClassName);
				bool openCreatePopup = false;

				const bool pushedErrorColor = (!validScriptClass && !component.ClassName.empty());
				if (pushedErrorColor)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.3f, 1.0f));

				const std::string namespacePrefix = ScriptEngine::GetProjectNamespace() + ".";

				auto displayName = [&](const std::string& fullName) -> std::string
					{
						if (fullName.rfind(namespacePrefix, 0) == 0)          // starts with "Sandbox."
							return fullName.substr(namespacePrefix.length());
						return fullName;                                      // foreign namespace — show in full
					};

				const std::string previewStr = component.ClassName.empty() ? "(none)" : displayName(component.ClassName);
				{
					ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
					ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
					ImGui::BeginTable("ScriptClass", 2, flags);
					ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
					ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Class");

					ImGui::TableSetColumnIndex(1);
					{
						// Reserve room for the open-script button so the combo doesn't
						// stretch under it and push it out of the cell.
						const float buttonWidth = ImGui::CalcTextSize(ICON_TOASTER_CODE).x + ImGui::GetStyle().FramePadding.x * 2.0f;
						ImGui::SetNextItemWidth(-(buttonWidth + ImGui::GetStyle().ItemSpacing.x));

						// "##Class" instead of "Class": the visible label now lives in column 0.
						if (ImGui::BeginCombo("##Class", previewStr.c_str()))
						{
							for (const auto& [fullName, scriptClass] : ScriptEngine::GetEntityClasses())
							{
								bool selected = (component.ClassName == fullName);
								// Display stripped, but keep the full name in the ID so two classes with
								// the same short name in different namespaces don't collide.
								std::string label = displayName(fullName) + "##" + fullName;
								if (ImGui::Selectable(label.c_str(), selected))
								{
									component.ClassName = fullName;               // always store qualified
									component.ScriptHandle = ScriptEngine::ResolveScriptHandleFromClass(fullName);
								}
								if (selected)
									ImGui::SetItemDefaultFocus();
							}

							ImGui::Separator();
							if (ImGui::Selectable(ICON_TOASTER_CODE " New Script..."))
								openCreatePopup = true;

							ImGui::EndCombo();
						}

						ImGui::SameLine();
						ImGui::BeginDisabled(!validScriptClass);
						if (ImGui::Button(ICON_TOASTER_CODE "##openScript"))
						{
							std::filesystem::path scriptPath = ScriptEngine::GetEntityClassSourcePath(component.ClassName);
							if (!scriptPath.empty() && mOpenScriptCallback)
								mOpenScriptCallback(mAssetRoot / scriptPath);
							else if (scriptPath.empty())
								TOAST_CORE_WARN("No source file found for script class '%s'", component.ClassName.c_str());
						}
						ImGui::EndDisabled();
					}

					ImGui::EndTable();
				}

				if (pushedErrorColor)
					ImGui::PopStyleColor();

				if (openCreatePopup)
					ImGui::OpenPopup("Create Script##scriptCreate");

				if (ImGui::BeginPopupModal("Create Script##scriptCreate", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					static char nameBuffer[64] = "";

					// Focus the field on open so the user can just start typing.
					if (ImGui::IsWindowAppearing())
					{
						nameBuffer[0] = '\0';        // static buffer is shared across entities
						ImGui::SetKeyboardFocusHere();
					}

					ImGui::InputText("Class Name", nameBuffer, sizeof(nameBuffer));

					std::string name = nameBuffer;
					std::filesystem::path target = mAssetRoot / "Scripts" / (name + ".cs");

					const char* error = nullptr;
					if (name.empty())                          
						error = "Class name required";
					else if (!ScriptEngine::IsValidIdentifier(name))  
						error = "Not a valid C# identifier";
					else if (std::filesystem::exists(target))  
						error = "A script with this name already exists";

					// Live preview: qualified name + destination path. This is what replaces
					ImGui::Separator();
					ImGui::TextDisabled("%s.%s", scriptNamespace.c_str(), name.empty() ? "..." : name.c_str());
					ImGui::TextDisabled("%s", target.string().c_str());

					if (error)
						ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.3f, 1.0f), "%s", error);

					ImGui::BeginDisabled(error != nullptr);
					if (ImGui::Button("Create"))
					{
						if (ScriptEngine::WriteScriptTemplate(target, scriptNamespace, name))
						{
							// Register the freshly written .cs in place (no copy — it's already in the
							// project). ImportAsset returns the handle; existing handle if already known.
							std::filesystem::path relative = std::filesystem::relative(target, mAssetRoot);
							AssetHandle handle = AssetManager::ImportAsset(relative);

							component.ScriptHandle = handle;

							// ClassName is deliberately NOT set here — the class doesn't exist in the
							// assembly until compiled, and an unresolvable ClassName crashes the fields

							if (mOpenScriptCallback)
								mOpenScriptCallback(target);
						}
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					ImGui::SameLine();
					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
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
								if (DrawFloatControl(PrettifyScriptFieldName(name), data, window, activeDragArea, 90.0f, 0.0f, 0.0f, 0.01f, "%.3f"))
									scriptInstance->SetFieldValue<float>(name, data);
							}
						}
					}
				}
				else
				{
					if (validScriptClass)
					{
						Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(component.ClassName);
						const auto& fields = entityClass->GetFields();

						auto& entityFields = ScriptEngine::GetScriptFieldMap(entity);

						std::vector<const std::string*> sortedNames;
						sortedNames.reserve(fields.size());
						for (const auto& [name, field] : fields)
							sortedNames.push_back(&name);

						std::sort(sortedNames.begin(), sortedNames.end(),
							[](const std::string* a, const std::string* b) { return *a < *b; });

						for (const std::string* namePtr : sortedNames)
						{
							const std::string& name = *namePtr;
							const ScriptField& field = fields.at(name);

							// Field has been set in the editor
							if (entityFields.find(name) != entityFields.end())
							{
								ScriptFieldInstance& scriptField = entityFields.at(name);

								if (field.Type == ScriptFieldType::Float)
								{
									float data = scriptField.GetValue<float>();
									if (DrawFloatControl(PrettifyScriptFieldName(name), data, window, activeDragArea, 90.0f, 0.0f, 0.0f, 0.01f, "%.3f"))
										scriptField.SetValue(data);
								}
							}
							else
							{
								// Display control to set it maybe
								if (field.Type == ScriptFieldType::Float)
								{
									float data = 0.0f;
									if (DrawFloatControl(PrettifyScriptFieldName(name), data, window, activeDragArea, 90.0f, 0.0f, 0.0f, 0.01f, "%.3f"))
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
			});

		DrawComponent<SceneScriptComponent>(ICON_TOASTER_CODE" Scene Script", entity, mScene, activeDragArea, mWindow, mAssetRoot, [=](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				bool scriptClassExists = ScriptEngine::EntityClassExists(component.ClassName);

				std::string scriptNamespace = SanitizeNamespace(mProjectName);
				bool validScriptClass = ScriptEngine::EntityClassExists(component.ClassName);
				bool openCreatePopup = false;

				// Track the push explicitly — the combo below can change ClassName mid-frame,
				// which would make a re-evaluated condition disagree with what we pushed.
				const bool pushedErrorColor = (!validScriptClass && !component.ClassName.empty());
				if (pushedErrorColor)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.3f, 1.0f));

				const std::string namespacePrefix = ScriptEngine::GetProjectNamespace() + ".";
				auto displayName = [&](const std::string& fullName) -> std::string
					{
						if (fullName.rfind(namespacePrefix, 0) == 0)
							return fullName.substr(namespacePrefix.length());
						return fullName;                                  // foreign namespace — show in full
					};

				const std::string previewStr = component.ClassName.empty() ? "(none)" : displayName(component.ClassName);

				{
					ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
					ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
					ImGui::BeginTable("SceneScriptClass", 2, flags);      // unique table id
					ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
					ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Class");
					ImGui::TableSetColumnIndex(1);
					{
						const float buttonWidth = ImGui::CalcTextSize(ICON_TOASTER_CODE).x + ImGui::GetStyle().FramePadding.x * 2.0f;
						ImGui::SetNextItemWidth(-(buttonWidth + ImGui::GetStyle().ItemSpacing.x));

						if (ImGui::BeginCombo("##SceneClass", previewStr.c_str()))
						{
							for (const auto& [fullName, scriptClass] : ScriptEngine::GetEntityClasses())
							{
								bool selected = (component.ClassName == fullName);
								std::string label = displayName(fullName) + "##" + fullName;

								if (ImGui::Selectable(label.c_str(), selected))
								{
									component.ClassName = fullName;
									component.ScriptHandle = ScriptEngine::ResolveScriptHandleFromClass(fullName);
								}
								if (selected)
									ImGui::SetItemDefaultFocus();
							}

							ImGui::Separator();
							if (ImGui::Selectable(ICON_TOASTER_CODE " New Script..."))
								openCreatePopup = true;

							ImGui::EndCombo();
						}

						ImGui::SameLine();
						ImGui::BeginDisabled(!validScriptClass);
						if (ImGui::Button(ICON_TOASTER_CODE "##openSceneScript"))   // unique button id
						{
							std::filesystem::path scriptPath = ScriptEngine::GetEntityClassSourcePath(component.ClassName);
							if (!scriptPath.empty() && mOpenScriptCallback)
								mOpenScriptCallback(mAssetRoot / scriptPath);       // relative -> absolute
							else if (scriptPath.empty())
								TOAST_CORE_WARN("No source file found for script class '%s'", component.ClassName.c_str());
						}
						ImGui::EndDisabled();
					}
					ImGui::EndTable();
				}

				if (pushedErrorColor)
					ImGui::PopStyleColor();

				if (openCreatePopup)
					ImGui::OpenPopup("Create Scene Script##sceneScriptCreate");    // unique popup id

				if (ImGui::BeginPopupModal("Create Scene Script##sceneScriptCreate", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					static char nameBuffer[64] = "";

					if (ImGui::IsWindowAppearing())
					{
						nameBuffer[0] = '\0';
						ImGui::SetKeyboardFocusHere();
					}

					ImGui::InputText("Class Name", nameBuffer, sizeof(nameBuffer));

					std::string name = nameBuffer;
					std::filesystem::path target = mAssetRoot / "Scripts" / (name + ".cs");

					const char* error = nullptr;
					if (name.empty())                                error = "Class name required";
					else if (!ScriptEngine::IsValidIdentifier(name)) error = "Not a valid C# identifier";
					else if (std::filesystem::exists(target))        error = "A script with this name already exists";

					ImGui::Separator();
					ImGui::TextDisabled("%s.%s", scriptNamespace.c_str(), name.empty() ? "..." : name.c_str());
					ImGui::TextDisabled("%s", target.string().c_str());

					if (error)
						ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.3f, 1.0f), "%s", error);

					ImGui::BeginDisabled(error != nullptr);
					if (ImGui::Button("Create"))
					{
						if (ScriptEngine::WriteScriptTemplate(target, scriptNamespace, name))
						{
							std::filesystem::path relative = std::filesystem::relative(target, mAssetRoot);
							component.ScriptHandle = AssetManager::ImportAsset(relative);

							// ClassName deliberately not set — the class doesn't exist until compiled.
							if (mOpenScriptCallback)
								mOpenScriptCallback(target);
						}
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					ImGui::SameLine();
					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
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
								if (DrawFloatControl(PrettifyScriptFieldName(name), data, window, activeDragArea, 90.0f, 0.0f, 0.0f, 0.01f, "%.3f"))
									scriptInstance->SetFieldValue<float>(name, data);
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
									if (DrawFloatControl(PrettifyScriptFieldName(name), data, window, activeDragArea, 90.0f, 0.0f, 0.0f, 0.01f, "%.3f"))
										scriptField.SetValue(data);
								}
							}
							else
							{
								// Display control to set it maybe
								if (field.Type == ScriptFieldType::Float)
								{
									float data = 0.0f;
									if (DrawFloatControl(PrettifyScriptFieldName(name), data, window, activeDragArea, 90.0f, 0.0f, 0.0f, 0.01f, "%.3f"))
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
			});

		DrawComponent<RigidBodyComponent>(ICON_TOASTER_HAND_ROCK_O" Rigid Body", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
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
				ImGuiHelpers::ManualDragDouble3("CenterOfMass", component.CenterOfMass, 1.0f, 0.0f, window, activeDragArea);

				ImGui::EndTable();

				float mass = 1.0f / (float)component.InvMass;
				if (DrawFloatControl("Mass (kg)", mass, window, activeDragArea, 90.0f, 0.0f, 60000.0f, 0.1f))
				{
					component.InvMass = 1.0f / mass;
					if (entity.HasComponent<BoxColliderComponent>())
						entity.GetComponent<BoxColliderComponent>().IsDirty = true;
					else if (entity.HasComponent<SphereColliderComponent>())
						entity.GetComponent<SphereColliderComponent>().IsDirty = true;
				}

				temp = static_cast<float>(component.Elasticity);
				if(DrawFloatControl("Elasticity (0-1)", temp, window, activeDragArea, 90.0f, 0.0f, 1.0f, 0.01f, "%.2f"))
					component.Elasticity = static_cast<double>(temp);

				temp = static_cast<float>(component.AngularDamping);
				if (DrawFloatControl("Angular Damping", temp, window, activeDragArea, 90.0f, 0.0f, 10.0f, 0.01f, "%.2f"))
					component.AngularDamping = static_cast<double>(temp);

				temp = static_cast<float>(component.StaticFriction);
				if(DrawFloatControl("Static Friction", temp, window, activeDragArea, 90.0f, 0.0f, 2.0f, 0.01f, "%.2f"))
					component.StaticFriction = static_cast<double>(temp);

				temp = static_cast<float>(component.DynamicFriction);
				if (DrawFloatControl("Dynamic Friction", temp, window, activeDragArea, 90.0f, 0.0f, 2.0f, 0.01f, "%.2f"))
					component.DynamicFriction = static_cast<double>(temp);

				DrawFloatControl("Drag Coefficient", component.DragCoefficient, window, activeDragArea, 90.0f, 0.0f, 10.0f, 0.01f, "%.2f");
				DrawFloatControl("Cross Section Min", component.CrossSectionMin, window, activeDragArea, 90.0f, 0.0f, 1000.0f, 0.1f, "%.2f");
				DrawFloatControl("Cross Section Max", component.CrossSectionMax, window, activeDragArea, 90.0f, 0.0f, 1000.0f, 0.1f, "%.2f");

				// Debug (read-only)
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				ImGui::Text("Aero Debug");
				ImGui::Text("Drag Force: (%.2f, %.2f, %.2f) |%.2f|",
					component.DebugDragForce.x, component.DebugDragForce.y, component.DebugDragForce.z,
					component.DebugDragForce.Length());
				ImGui::Text("Air Density: %.6f kg/m3", component.DebugAirDensity);
				ImGui::Text("Effective Cross Section: %.2f m2", component.DebugEffectiveCrossSection);
				ImGui::Text("Altitude: %.1f m", component.DebugAltitude);
				ImGui::Text("Linear Velocity: (%.2f, %.2f, %.2f) |%.2f|", component.LinearVelocity.x, component.LinearVelocity.y, component.LinearVelocity.z,
					component.LinearVelocity.Length());

			});

		DrawComponent<SphereColliderComponent>(ICON_TOASTER_CIRCLE_O" Sphere Collider", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				float temp;

				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::Checkbox("Render Collider", &component.RenderCollider);

				temp = static_cast<float>(component.Collider->mRadius);
				if (DrawFloatControl("Radius", temp, window, activeDragArea, 90.0f, 0.0f, 600.0f, 0.1f, "%.4f"))
				{
					component.Collider->mRadius = static_cast<double>(temp);

					component.Collider->CalculateBounds();
					component.IsDirty = true;
				}
			});

		DrawComponent<BoxColliderComponent>(ICON_TOASTER_CUBE" Box Collider", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
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
				if (ImGuiHelpers::ManualDragDouble3("Size", component.Collider->mSize, 1.0f, 0.0f, window, activeDragArea))
				{
					component.Collider->CalculateBounds();
					component.Collider->BuildCornerPoints();

					component.IsDirty = true;
				}
				ImGui::EndTable();
			});

		DrawComponent<UIPanelComponent>(ICON_TOASTER_SQUARE_O" UI Panel", entity, mScene, activeDragArea, mWindow, mAssetRoot, [this](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("##panelTable", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);

				Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

				if (component.TextureHandle != AssetHandle(0))
				{
					auto tex = AssetManager::GetAsset<Texture2D>(component.TextureHandle);
					if (tex)
						displayTexture = tex.get();
				}

				ImGui::Image(displayTexture->GetID(), { 64.0f, 64.0f });

				std::optional<std::string> filepath;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = assetRoot / path;
						filepath = completePath.string();

						if (filepath)
						{
							RequestUITextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
								{
									auto& comp = entity.GetComponent<UIPanelComponent>();
									uint32_t sliceIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(handle);
									comp.TextureHandle = handle;
									comp.TextureIndex = sliceIndex;
								});
						}
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					auto texturePath = mAssetRoot / "Textures" / "UI";
					filepath = FileDialogs::OpenFile("", texturePath.string().c_str());

					if (filepath)
					{
						RequestUITextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
							{
								auto& comp = entity.GetComponent<UIPanelComponent>();
								uint32_t sliceIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(handle);
								comp.TextureHandle = handle;
								comp.TextureIndex = sliceIndex;
							});
					}
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::BeginTable("##table2", 2, flags);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Checkbox("Use##Color", &component.UseColor);
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##color", &component.Color.x);
				ImGui::EndTable();
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Corner Radius");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##cornerradius", &component.CornerRadius, 0.0f, 50.0f, "%.1f");

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Visible");
				ImGui::TableSetColumnIndex(1);
				ImGui::Checkbox("##visible", &component.Visible);
				ImGui::TableNextRow();

				if (entity.HasParent())
				{
					float wrapWidth = 90.0f - ImGui::GetStyle().ItemSpacing.x;

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Connect to parent");
					ImGui::TableSetColumnIndex(1);
					ImGui::Checkbox("##connecttoparent", &component.ConnectToParent);
				}

				if(component.ConnectToParent)
				{
					float wrapWidth = 90.0f - ImGui::GetStyle().ItemSpacing.x;

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Connector Color");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::ColorEdit4("##connectorcolor", &component.Connector.Color.x);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Connector Thickness");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::SliderFloat("##ConnectorThickness", &component.Connector.Thickness, 0.0f, 50.0f, "%.1f");

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Connector Child Offset");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGuiHelpers::ManualDragFloat2("##ConnectorChildOffset", component.Connector.ChildOffset, 0.1f, 0.0f, window, activeDragArea, "%.1f", false);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Connector Parent Offset");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGuiHelpers::ManualDragFloat2("##ConnectorParentOffset", component.Connector.ParentOffset, 0.1f, 0.0f, window, activeDragArea, "%.1f", false);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Connector Style");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					const char* connectorStyles[] = { "Straight", "Elbow H", "Elbow V" };
					int styleIndex = (int)component.Connector.Style;
					if (ImGui::Combo("##ConnectorStyle", &styleIndex, connectorStyles, IM_ARRAYSIZE(connectorStyles)))
						component.Connector.Style = (ConnectorStyle)styleIndex;

					if (component.Connector.Style != ConnectorStyle::Straight)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
						ImGui::TextUnformatted("Connector Corner Radius");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-1);
						ImGui::SliderFloat("##ConnectorCornerRadius", &component.Connector.CornerRadius, 0.0f, 50.0f, "%.1f");
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Connector Outline Width");
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);
					ImGui::SliderFloat("##ConnectorOutlineWidth", &component.Connector.OutlineWidth, 0.0f, 20.0f, "%.1f");

					if (component.Connector.OutlineWidth > 0.0f)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
						ImGui::TextUnformatted("Connector Outline Color");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-1);
						ImGui::ColorEdit3("##ConnectorOutlineColor", &component.Connector.OutlineColor.x);
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
					ImGui::TextUnformatted("Clamp To Screen Edge");
					ImGui::TableSetColumnIndex(1);
					ImGui::Checkbox("##ConnectorClampToEdge", &component.Connector.ClampToScreenEdge);

					if (component.Connector.ClampToScreenEdge)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
						ImGui::TextUnformatted("Screen Edge Margin");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-1);
						ImGui::SliderFloat("##ConnectorEdgeMargin", &component.Connector.ScreenEdgeMargin, 0.0f, 200.0f, "%.0f");
					}
				}

				ImGui::EndTable();
			});

		DrawComponent<UITextComponent>(ICON_TOASTER_FILE_TEXT" UI Text", entity, mScene, activeDragArea, mWindow, mAssetRoot, [](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				auto& text = component.Text;

				char buffer[1024 * 5];
				memset(buffer, 0, sizeof(buffer));
				strncpy_s(buffer, text.c_str(), sizeof(buffer));
				ImGuiTableFlags textFlags = ImGuiInputTextFlags_CtrlEnterForNewLine;
				if (ImGui::InputTextMultiline("##text", buffer, IM_ARRAYSIZE(buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), textFlags)) 
				{
					text = std::string(buffer);
					component.Text = text;
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
				if (!component.Font->GetFilePath().empty())
					ImGui::InputText("##fontfilepath", (char*)component.Font->GetFilePath().c_str(), 256, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##fontfilepath", (char*)"Empty", 256, ImGuiInputTextFlags_ReadOnly);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("...##openfont"))
				{
					std::optional<std::string> filepath = FileDialogs::OpenFile("*.ttf", "..\\Toaster\\assets\\fonts\\");
					if (filepath)
					{
						component.Font = CreateRef<Font>(*filepath);

						uint32_t sliceIndex = Renderer2D::GetRendererData()->FontsTextureArray->GetSliceIndexForTextureOLD(*filepath);
						component.TextureIndex = sliceIndex;
					}
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##color", &component.Color.x);
				ImGui::TableSetColumnIndex(0);

				ImGui::PopItemWidth();

				ImGui::EndTable();
			});

		DrawComponent<UIButtonComponent>(ICON_TOASTER_SQUARE_O" UI Button", entity, mScene, activeDragArea, mWindow, mAssetRoot, [this](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("UIButtonComponent", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.7f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);
				ImGui::TextWrapped("Texture");
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);

				Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

				if (component.TextureHandle != AssetHandle(0))
				{
					auto tex = AssetManager::GetAsset<Texture2D>(component.TextureHandle);
					if (tex)
						displayTexture = tex.get();
				}

				ImGui::Image(displayTexture->GetID(), { 64.0f, 64.0f });

				std::optional<std::string> filepath;

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = assetRoot / path;
						filepath = completePath.string();

						if (filepath)
						{
							RequestUITextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
								{
									auto& comp = entity.GetComponent<UIButtonComponent>();
									uint32_t sliceIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(handle);
									comp.TextureHandle = handle;
									comp.TextureIndex = sliceIndex;
								});
						}
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					auto texturePath = mAssetRoot / "Textures" / "UI";
					filepath = FileDialogs::OpenFile("", texturePath.string().c_str());

					if (filepath)
					{
						RequestUITextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
							{
								auto& comp = entity.GetComponent<UIButtonComponent>();
								uint32_t sliceIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(handle);
								comp.TextureHandle = handle;
								comp.TextureIndex = sliceIndex;
							});
					}
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::BeginTable("##table2", 2, flags);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Checkbox("Use##Color", &component.UseColor);
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##buttoncolor", &component.Color.x);
				ImGui::EndTable();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);
				ImGui::TextWrapped("Click Texture");
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);

				displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

				if (component.ClickTextureHandle != AssetHandle(0))
				{
					auto tex = AssetManager::GetAsset<Texture2D>(component.ClickTextureHandle);
					if (tex)
						displayTexture = tex.get();
				}

				ImGui::Image(displayTexture->GetID(), { 64.0f, 64.0f });

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = assetRoot / path;
						filepath = completePath.string();

						if (filepath)
						{
							RequestUITextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
								{
									auto& comp = entity.GetComponent<UIButtonComponent>();
									uint32_t sliceIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(handle);
									comp.ClickTextureHandle = handle;
									comp.ClickTextureIndex = sliceIndex;
								});
						}
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					auto texturePath = mAssetRoot / "Textures" / "UI";
					filepath = FileDialogs::OpenFile("", texturePath.string().c_str());

					if (filepath)
					{
						RequestUITextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
							{
								auto& comp = entity.GetComponent<UIButtonComponent>();
								uint32_t sliceIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(handle);
								comp.ClickTextureHandle = handle;
								comp.ClickTextureIndex = sliceIndex;
							});
					}
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::BeginTable("##table2", 2, flags);
				ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 1.1f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Checkbox("Use##ColorClick", &component.UseColor);
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##buttonclickcolor", &component.ClickColor.x);
				ImGui::EndTable();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped("Corner Radius");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::SliderFloat("##cornerradius", &component.CornerRadius, 0.0f, 50.0f, "%.1f");

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Visible");
				ImGui::TableSetColumnIndex(1);
				ImGui::Checkbox("##visible", &component.Visible);
				ImGui::TableNextRow();

				ImGui::EndTable();
			});

		DrawComponent<ParticlesComponent>(ICON_TOASTER_SNOWFLAKE" Particles", entity, mScene, activeDragArea, mWindow, mAssetRoot, [this](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::BeginTable("##ParticlesComponent", 2, flags);

				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.30f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.70f);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped("Emitting");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				ImGui::Checkbox("##emittingCheckbox", &component.Emitting);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped("Emit Function");
				ImGui::TableSetColumnIndex(1);
				int currentSpawnFunction = static_cast<int>(component.SpawnFunction);
				const char* emitFuncs[] = { "None", "Cone", "Box", "Disc" };
				if (ImGui::Combo("##emitFunction", &currentSpawnFunction, emitFuncs, IM_ARRAYSIZE(emitFuncs)))
				{
					component.SpawnFunction = static_cast<EmitFunction>(currentSpawnFunction);
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped("Blend Mode");
				ImGui::TableSetColumnIndex(1);
				int currentBlendMode = static_cast<int>(component.BlendMode);
				const char* blendModes[] = { "Additive (emissive)", "Alpha (occluding)" };
				if (ImGui::Combo("##blendMode", &currentBlendMode, blendModes, IM_ARRAYSIZE(blendModes)))
					component.BlendMode = static_cast<ParticleBlendMode>(currentBlendMode);

				ImGui::EndTable();

				ImGui::BeginTable("##ColorPickers", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.30);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.70f);

				// Start Color Picker
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped("Start Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::ColorEdit3("##StartColor", &component.StartColor.x);

				// End Color Picker
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped("End Color");
				ImGui::TableSetColumnIndex(1);
				ImGui::ColorEdit3("##EndColor", &component.EndColor.x);

				ImGui::EndTable();

				// HDR intensity ramp
				DrawFloatControl("Start Intensity", component.StartIntensity, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 50.0f, 0.1f, "%.2f");

				DrawFloatControl("End Intensity", component.EndIntensity, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 10.0f, 0.05f, "%.2f");

				DrawFloatControl("Intensity Falloff", component.IntensityFalloff, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.1f, 5.0f, 0.05f, "%.2f");

				DrawFloatControl("Size", component.Size, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 10.0f, 0.01f, "%.2f");

				DrawFloatControl("Color Blend Factor", component.ColorBlendFactor, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 1.0f, 0.01f, "%.2f");

				DrawFloatControl("Alpha Scale", component.AlphaScale, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 1.0f, 0.01f, "%.2f");

				DrawFloatControl("Max life time", component.MaxLifeTime, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 100.0f, 0.1f, "%.3f");

				DrawFloatControl("Spawn delay", component.SpawnDelay, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.000001f, 10.0f, 0.00001f, "%.6f");

				ImGuiHelpers::ManualDragFloat3("Velocity", component.Velocity, 1.0f, 0.0f, window, activeDragArea);

				DrawFloatControl("Grow Rate", component.GrowRate, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 10.0f, 0.1f, "%.1f");

				DrawFloatControl("Burst Initial", component.BurstInitial, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 50.0f, 0.1f, "%.1f");

				DrawFloatControl("Burst Decay", component.BurstDecay, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 100.0f, 0.1f, "%.1f");

				DrawFloatControl("Inherit Velocity", component.InheritVelocityScale, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 1.0f, 0.01f, "%.2f");

				// Per-particle jitter
				DrawFloatControl("Speed Jitter", component.SpeedJitter, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 1.0f, 0.01f, "%.2f");

				// Stops the tail cutting off where an entire cohort dies at once.
				DrawFloatControl("Lifetime Jitter", component.LifetimeJitter, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 1.0f, 0.01f, "%.2f");

				// Breaks the "identical stamped shapes" read.
				DrawFloatControl("Size Jitter", component.SizeJitter, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 1.0f, 0.01f, "%.2f");

				DrawFloatControl("Directional Jitter", component.DirectionalJitter, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 45.0f, 0.1f, "%.1f");

				// Soft particles
				DrawFloatControl("Soft Fade Distance", component.SoftFadeDistance, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 100.0f, 0.05f, "%.2f");

				DrawFloatControl("Drag", component.Drag, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 5.0f, 0.01f, "%.3f");

				DrawFloatControl("Turbulence", component.TurbulenceStrength, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 20.0f, 0.05f, "%.2f");

				ImGuiHelpers::ManualDragFloat3("Spawn Offset", component.SpawnOffset, 0.1f, 0.0f, window, activeDragArea);

				if (component.SpawnFunction == EmitFunction::CONE)
				{
					DrawFloatControl("Cone Angle (deg)", component.ConeAngleDegrees, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 180.0f, 0.1f, "%.1f");
				}
				else if (component.SpawnFunction == EmitFunction::BOX)
				{
					ImGuiHelpers::ManualDragFloat3("Spawn Box Size", component.SpawnBoxSize, 0.1f, 0.0f, window, activeDragArea);

					DrawFloatControl("Box Bias Exponent", component.BiasExponent, window, activeDragArea, contentRegionAvailable.x * 0.30, 1.0f, 10.0f, 0.1f, "%.1f");
				}
				if (component.SpawnFunction == EmitFunction::DISC)
				{
					DrawFloatControl("Disc Radius", component.SpawnBoxSize.x, window, activeDragArea, contentRegionAvailable.x * 0.30, 0.0f, 200.0f, 0.5f, "%.1f");
					// Velocity means something different here - say so.
					ImGui::TextDisabled("Velocity: X = outward, Y = upward");
				}

				ImGui::BeginTable("##textures", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.30);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x * 0.70f);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped("Mask Texture");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);

				Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

				if (component.MaskTextureHandle != AssetHandle(0))
				{
					auto tex = AssetManager::GetAsset<Texture2D>(component.MaskTextureHandle);
					if (tex)
						displayTexture = tex.get();
				}

				ImGui::Image(displayTexture->GetID(), { 64.0f, 64.0f });

				std::optional<std::string> filepath;
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						auto completePath = assetRoot / path;
						filepath = completePath.string();

						if (filepath)
						{ 
							RequestTextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
								{
									auto& comp = entity.GetComponent<ParticlesComponent>();
									comp.MaskTextureHandle = handle;
								});
						}
					}

					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
				{
					auto texturePath = mAssetRoot / "Textures";
					filepath = FileDialogs::OpenFile("", texturePath.string().c_str());

					if (filepath)
					{
						RequestTextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
							{
								auto& comp = entity.GetComponent<ParticlesComponent>();
								comp.MaskTextureHandle = handle;
							});
					}
				}

				ImGui::EndTable();
			});

			DrawComponent<MoveableComponent>(ICON_TOASTER_LOCATION_ARROW" Movable", entity, mScene, activeDragArea, mWindow, mAssetRoot, [this](auto& component, Entity entity, Scene* scene, WindowsWindow* window, std::string& activeDragArea, std::filesystem::path& assetRoot)
				{
					// Active toggle — gates whether the entity accepts MoveTo
					ImGui::Checkbox("Active", &component.IsActive);

					DrawFloatControl("Ground Offset", component.GroundOffset, window, activeDragArea, 90.0f, -500.0f, 500.0f, 0.05f, "%.2f");

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();
					ImGui::Text("Marker");

					// Marker texture (AssetHandle drop target) — see note below, match your existing texture-drop pattern
					{
						ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
						ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
						ImGui::BeginTable("MoveableMarkerTexture", 2, flags);
						ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
						ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, contentRegionAvailable.x - 90.0f);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Texture");
						ImGui::TableSetColumnIndex(1);

						Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

						// Show current handle / a button to clear, plus a drag-drop accept target.
						if (component.MarkerTextureHandle != AssetHandle(0))
						{
							auto tex = AssetManager::GetAsset<Texture2D>(component.MarkerTextureHandle);
							if (tex)
								displayTexture = tex.get();
						}

						ImGui::Image(displayTexture->GetID(), { 64.0f, 64.0f });

						std::optional<std::string> filepath;
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
							{
								const wchar_t* path = (const wchar_t*)payload->Data;
								auto completePath = assetRoot / path;
								filepath = completePath.string();

								if (filepath)
								{
									RequestTextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
										{
											auto& comp = entity.GetComponent<MoveableComponent>();
											comp.MarkerTextureHandle = handle;
										});
								}
							}

							ImGui::EndDragDropTarget();
						}

						if (ImGui::IsItemClicked())
						{
							auto texturePath = mAssetRoot / "Textures";
							filepath = FileDialogs::OpenFile("", texturePath.string().c_str());

							if (filepath)
							{
								RequestTextureImport(*filepath, false, [this, entity](AssetHandle handle) mutable
									{
										auto& comp = entity.GetComponent<MoveableComponent>();
										comp.MarkerTextureHandle = handle;
									});
							}
						}

						ImGui::EndTable();
					}

					DrawFloatControl("Size", component.MarkerSize, window, activeDragArea, 90.0f, 0.1f, 50.0f, 0.05f, "%.2f");
					DrawFloatControl("Fade Out Duration", component.MarkerFadeOutDuration, window, activeDragArea, 90.0f, 0.0f, 10.0f, 0.05f, "%.2f");
				});
	}

}