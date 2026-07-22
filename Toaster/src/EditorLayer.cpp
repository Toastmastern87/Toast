#include "EditorLayer.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

#include "Toast/Core/Application.h"
#include "Toast/Core/Input.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Project/ProjectSerializer.h"

#include "Toast/Scene/SceneSerializer.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Utils/PlatformUtils.h"

#include "imgui/imgui_internal.h"
#include "imgui/imgui.h"

#include <filesystem>

#include "FontAwesome.h"

#include "../vendor/ImGuizmo/src/ImGuizmo.h"

namespace Toast {

	static std::optional<std::filesystem::path> FindProjectFileInFolder(const std::filesystem::path& folder)
	{
		if (!std::filesystem::exists(folder) || !std::filesystem::is_directory(folder))
			return std::nullopt;

		for (const auto& entry : std::filesystem::directory_iterator(folder))
		{
			if (!entry.is_regular_file())
				continue;

			const auto& p = entry.path();
			if (p.extension() == ".tproj") // <- your extension
				return p;
		}

		return std::nullopt;
	}

	EditorLayer::EditorLayer(WindowsWindow* window)
		: Layer("TheNextFrontier2D", window)
	{
		mWindow = window;
	}

	void EditorLayer::OnAttach()
	{
		TOAST_PROFILE_FUNCTION();

		Application::Get().SetSceneProvider(this);

		// Standard Textures
		mCheckerboardTexture = TextureLibrary::LoadTexture2D("assets/textures/Checkerboard.png");
		mPlayButtonTex = TextureLibrary::LoadTexture2D("..\\Toaster/Resources/Icons/PlayButton.png");
		mPauseButtonTex = TextureLibrary::LoadTexture2D("..\\Toaster/Resources/Icons/PauseButton.png");
		mStopButtonTex = TextureLibrary::LoadTexture2D("..\\Toaster/Resources/Icons/StopButton.png");
		mCloseButtonTex = TextureLibrary::LoadTexture2D("..\\Toaster/Resources/Icons/CloseWindowButton.png");
		mMinButtonTex = TextureLibrary::LoadTexture2D("..\\Toaster/Resources/Icons/MinWindowButton.png");
		mMaxButtonTex = TextureLibrary::LoadTexture2D("..\\Toaster/Resources/Icons/MaxWindowButton.png");
		mLogoTex = TextureLibrary::LoadTexture2D("..\\Toaster/Resources/Icons/ToasterIcon48x48.png");

		TextureLibrary::LoadTexture2D("assets/textures/White.png");
		TextureLibrary::LoadTextureCube("assets/textures/WhiteCube.png", 1, 1);

		//mPlaceholderScene = CreateScope<Scene>();
		//mEditorScene = mPlaceholderScene.get();

		//mEditorCamera = CreateRef<EditorCamera>(30.0f, 1.778f, 0.1f, 3000000.0f);
		//mEditorCamera->SetTranslation({ 0.0f, 1.0f, -3.0f });

		//mEditorScene->SetActiveCamera(mEditorCamera);
		//mEditorCamera->UpdateView();

		//mProjectPanel.OnOpenSceneRequested = [this](UUID id) { OpenProjectScene(id); };

		// TEMP WHILE I'M NOT WORKING ON THE PROJECT OPENING SYSTEM, AUTO OPENS THE NEXT FRONTIER
#ifdef TRUE
		mForceProjectPopup = false;

		const std::filesystem::path autoFolder = R"(C:\dev\Toast\Toaster\assets\projects\The Next Frontier)";

		if (!std::filesystem::exists(autoFolder))
			TOAST_CORE_WARN("AUTO Project folder does not exist: %s", autoFolder.string().c_str());

		auto projectFileOpt = FindProjectFileInFolder(autoFolder);
		if (!projectFileOpt)
			TOAST_CORE_WARN("No .tproj file found in folder: %s", autoFolder.string().c_str());

		auto loadedProject = std::make_shared<Project>();

		ProjectSerializer serializer(loadedProject.get());
		if (!serializer.Deserialize(projectFileOpt->string()))
			TOAST_CORE_ERROR("Failed to load project: %s", projectFileOpt->string().c_str());

		// Success: swap active project
		mProject = loadedProject;

		AssetManager::SetActiveProject(mProject);
		AssetManager::DeserializeRegistry();

		Renderer::LoadEngineShaders();
		Renderer::GenerateSpecularBRDF();

		mPlaceholderScene = CreateScope<Scene>();
		mEditorScene = mPlaceholderScene.get();

		mEditorCamera = CreateRef<EditorCamera>(30.0f, 1.778f, 0.1f, 3000000.0f);
		mEditorCamera->SetTranslation({ 0.0f, 1.0f, -3.0f });

		mEditorScene->SetActiveCamera(mEditorCamera);
		mEditorCamera->UpdateView();

		mProjectPanel.OnOpenSceneRequested = [this](UUID id) { OpenProjectScene(id); };

		mContentBrowserPanel.SetProjectPath(mProject->GetPath());
		mMaterialPanel.SetProjectPath(mProject->GetPath());
		mPlanetPanel.SetProjectPath(mProject->GetPath());
		mPropertiesPanel.SetProjectPath(mProject->GetPath());

		Renderer2D::LoadUITextures();

		// Open active scene from the loaded project
		const std::filesystem::path scenePath = mProject->GetPath() / mProject->GetActiveScenePath();
		mSceneFilePath = scenePath.string();

		OpenScene(scenePath);

		SetContexts();

		mPropertiesPanel.SetOpenScriptCallback([this](const std::filesystem::path& path)
			{
				mScriptEditorPanel.OpenFile(path);
				mScriptEditorPanel.SetOpen(true);   
			});

		// If you have a “force popup to choose projects behavior, disable it on success
		mForceProjectPopup = false;

		TOAST_CORE_INFO("Opened project: %s", mProject->GetName().c_str());
		TOAST_CORE_INFO("Opened scene: %s", scenePath.string().c_str());
#endif 

	}

	void EditorLayer::OnDetach()
	{
		TOAST_PROFILE_FUNCTION();

		Application::Get().ClearSceneProvider(this);
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		TOAST_PROFILE_FUNCTION();

		if (mPendingSceneChangeName.has_value())
		{
			const std::string requested = *mPendingSceneChangeName;
			mPendingSceneChangeName.reset();

			if (!mProject)
			{
				TOAST_CORE_WARN("Scene change requested ('%s') but no project is loaded.", requested.c_str());
			}
			else
			{
				// Find scene UUID by display name (filename stem).
				// Implement this helper in Project (shown below).
				UUID id = mProject->FindSceneByDisplayName(requested);

				if (!id)
				{
					TOAST_CORE_WARN("Scene '%s' not found in project.", requested.c_str());
				}
				else
				{
					// Decide behaviour depending on state:
					// If playing, switch runtime scene; if editing, switch editor scene.
					if (mSceneState == SceneState::Play || mSceneState == SceneState::Pause)
						SwitchRuntimeToProjectScene(id);
					else
						OpenProjectScene(id); // your existing function (edit mode)
				}
			}
		}

		if (mEditorScene)
		{
			if (mViewportSize.x > 0.0f && mViewportSize.y > 0.0f)
			{
				mEditorCamera->SetViewportSize(mViewportSize.x, mViewportSize.y);
				mEditorScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			}

			Ref<RenderTarget>& positionRT = Renderer::GetGPassPositionRT();

			auto [width, height] = positionRT->GetSize();
			if (mViewportSize.x > 0.0f && mViewportSize.y > 0.0f && (width != mViewportSize.x || height != mViewportSize.y))
			{
				switch (mSceneState)
				{
				case SceneState::Edit:
				{
					mEditorCamera->SetViewportSize(mViewportSize.x, mViewportSize.y);
					mEditorScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

					break;
				}
				case SceneState::Play:
				{
					mRuntimeScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
					break;
				}
				}
			}

			// Update scene
			switch (mSceneState)
			{
			case SceneState::Edit:
			{
				// Update
				if (mViewportHovered)
					mEditorCamera->OnUpdate(ts);

				mEditorScene->OnUpdateEditor(ts, mEditorCamera);
				if(mViewportSize.x > 0 && mViewportSize.y > 0)
					mEditorScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

				break;
			}
			case SceneState::Play:
			{
				if (mBlockRuntimeFrames > 0)
				{
					mRuntimeScene->SetRuntimeBlocked(true);
					--mBlockRuntimeFrames;
					if (mBlockRuntimeFrames == 0)
						mRuntimeScene->SetRuntimeBlocked(false);
				}

				mRuntimeScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
				mRuntimeScene->SetViewportPos(mAbsoluteViewportPos);
				mRuntimeScene->OnUpdateRuntime(ts);

				break;
			}
			}
		}
	}

	void EditorLayer::OnImGuiRender()
	{
		TOAST_PROFILE_FUNCTION();

		static bool dockingEnabled = true;
		if (dockingEnabled)
		{
			static bool dockspaceOpen = true;
			static bool opt_fullscreen_persistant = true;
			bool opt_fullscreen = opt_fullscreen_persistant;
			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

			//ImGui::ShowDemoWindow();

			// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
			// because it would be confusing to have two docking targets within each others.
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
			if (opt_fullscreen)
			{
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->Pos);
				ImGui::SetNextWindowSize(viewport->Size);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar
					| ImGuiWindowFlags_NoCollapse
					| ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoMove
					| ImGuiWindowFlags_NoBringToFrontOnFocus
					| ImGuiWindowFlags_NoNavFocus
					| ImGuiWindowFlags_NoScrollbar
					| ImGuiWindowFlags_NoScrollWithMouse;
			}

			// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
			if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
				window_flags |= ImGuiWindowFlags_NoBackground;

			// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
			// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
			// all active windows docked into it will lose their parent and become undocked.
			// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
			// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("DockSpace Demo", nullptr, window_flags);
			ImGui::PopStyleVar();

			RenderCustomTitleBar();

			const bool wantPopup = mForceProjectPopup || mShowProjectPopup;

			if (wantPopup && !ImGui::IsPopupOpen("ProjectPopup"))
			{
				mNewProjectName[0] = '\0';
				mNewProjectLocation[0] = '\0';
				mOpenProjectPath[0] = '\0';

				ImGui::OpenPopup("ProjectPopup");
			}

			ShowProjectPopup(!mForceProjectPopup);

			if (mShowProjectPopup && ImGui::IsPopupOpen("ProjectPopup"))
				mShowProjectPopup = false;

			if (mForceProjectPopup)
			{
				// Optional: block the rest of the editor UI so user cannot interact with anything else
				ImGui::End(); // end "DockSpace Demo"
				if (opt_fullscreen) ImGui::PopStyleVar(2);
				return;
			}

			if (opt_fullscreen)
				ImGui::PopStyleVar(2);

			// DockSpace
			ImGuiIO& io = ImGui::GetIO();
			ImGuiStyle& style = ImGui::GetStyle();

			if (mWindow->IsDragging())
				io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
			else
				io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

			style.WindowMenuButtonPosition = ImGuiDir_None;
			float minWinSize = style.WindowMinSize.x;
			style.WindowMinSize.x = 370.0f;

			ImGui::BeginChild("DockSpaceRegion", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse	| ImGuiWindowFlags_NoDecoration);

			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

			ImGui::EndChild();

			style.WindowMinSize.x = minWinSize;

			ImGuiWindowClass windowClass;
			windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;

			ImGui::SetNextWindowClass(&windowClass);

			if (mEditorScene)
			{
				mSceneSettingsPanel.OnImGuiRender(&mShowSceneSettingsPopup, mActiveDragArea);
				mSceneHierarchyPanel.OnImGuiRender();
				mMaterialPanel.OnImGuiRender();
				mEnvironmentPanel.OnImGuiRender();
				mContentBrowserPanel.OnImGuiRender();
				mConsolePanel.OnImGuiRender();
				mPropertiesPanel.OnImGuiRender(mActiveDragArea);
				mPlanetPanel.OnImGuiRender(&mShowPlanetPopup, mActiveDragArea);
				mProjectPanel.OnImGuiRender();
				mScriptEditorPanel.OnImGuiRender();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
			ImGui::Begin(ICON_TOASTER_GAMEPAD" Viewport");

			mViewportFocused = ImGui::IsWindowFocused();
			mViewportHovered = ImGui::IsWindowHovered();
			Application::Get().GetImGuiLayer()->BlockEvents(!mViewportFocused && !mViewportHovered);

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

			ImVec2 windowPos = ImGui::GetWindowPos();
			auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
			auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
			ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
			ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
			mAbsoluteViewportPos = DirectX::XMFLOAT2(windowPos.x + contentMin.x, windowPos.y + contentMin.y);

			//mViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
			mViewportSize = { (contentMax.x - contentMin.x), (contentMax.y - contentMin.y) };

			mViewportBounds[0] = { viewportMinRegion.x + windowPos.x, viewportMinRegion.y + windowPos.y };
			mViewportBounds[1] = { viewportMaxRegion.x + windowPos.x, viewportMaxRegion.y + windowPos.y };

			if (mProject)
			{
				Scene* active = GetActiveScene();
				if (active)
					active->SetViewportBounds(mViewportBounds);
			}

			if (mViewportSize.x != mPreviousViewportSize.x || mViewportSize.y != mPreviousViewportSize.y)
				Renderer::OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

			void* textureID = nullptr;

			if (mEditorScene)
			{
				switch (mEditorScene->GetSettings().RenderOverlaySetting)
				{
				case RenderOverlay::NONE:
					textureID = (void*)Renderer::GetFinalEditorRT()->GetSRV().Get();
					break;
				case RenderOverlay::POSITIONS:
					textureID = (void*)Renderer::GetGPassPositionRT()->GetSRV().Get();
					break;
				case RenderOverlay::NORMALS:
					textureID = (void*)Renderer::GetGPassNormalRT()->GetSRV().Get();
					break;
				case RenderOverlay::ALBEDOMETALLIC:
					textureID = (void*)Renderer::GetGPassAlbedoMetallicRT()->GetSRV().Get();
					break;
				case RenderOverlay::ROUGHNESS:
					textureID = (void*)Renderer::GetGPassRoughnessAORT()->GetSRV().Get();
					break;
				case RenderOverlay::LPASS:
					textureID = (void*)Renderer::GetLPassRT()->GetSRV().Get();
					break;
				case RenderOverlay::ATMOSPHERICSCATTERING:
					textureID = (void*)Renderer::GetAtmosphericScatteringRT()->GetSRV().Get();
					break;
				case RenderOverlay::SSAO:
					textureID = (void*)Renderer::GetSSAORT()->GetSRV().Get();
					break;
				case RenderOverlay::SSAOBLUR:
					textureID = (void*)Renderer::GetSSAOBlurRT()->GetSRV().Get();
					break;
				case RenderOverlay::BLOOM:
					textureID = (void*)Renderer::GetSunBloomRT()->GetSRV().Get();
					break;
				case RenderOverlay::BLOOMHALF:
					textureID = (void*)Renderer::GetSunBloomHalfRT()->GetSRV().Get();
					break;
				case RenderOverlay::BLOOMQUARTER:
					textureID = (void*)Renderer::GetSunBloomQuarterRT()->GetSRV().Get();
					break;
				case RenderOverlay::BLOOMFINAL:
					textureID = (void*)Renderer::GetFinalBloomRT()->GetSRV().Get();
					break;
				case RenderOverlay::SKYVIEWLUT:
					textureID = (void*)mEditorScene->GetPlanet()->GetSkyViewLUT()->GetSRV().Get();
					break;
				case RenderOverlay::PLANETMATERIALS:
					textureID = (void*)Renderer::GetPlanetMaterialDebugRT()->GetSRV().Get();
					break;
				}
			}
			else
			{
				// Show some placeholder RT or nothing
				textureID = (void*)mCheckerboardTexture->GetSRV().Get(); // or a checkerboard
			}

			if(mEditorScene)
				ImGui::Image((ImTextureID)textureID, ImVec2{mViewportSize.x, mViewportSize.y});

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					auto completePath = std::filesystem::path(mProject->GetPath() / path);
					std::string filename = completePath.string();

					// Get the file extension in lowercase
					std::string extension = completePath.extension().string();
					std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

					if (extension == ".ptoast")
					{
						std::string prefabName = completePath.stem().string();
						mEditorScene->AddPrefab(prefabName);
					}
					else if (extension == ".toast")
						OpenScene(completePath);
					else
						TOAST_CORE_WARN("Unhandled file type dropped: %s", filename.c_str());
				}

				ImGui::EndDragDropTarget();
			}

			if (mSceneState == SceneState::Edit)
			{
				// Overlay icons for quick settings access
				ImVec2 viewportImageMin = ImGui::GetItemRectMin();
				ImVec2 viewportImageMax = ImGui::GetItemRectMax();
				float overlayPadding = 10.0f;
				float iconSize = 30.0f;

				ImGui::PushFont(io.Fonts->Fonts[3]);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.25f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.5f));

				ImGui::SetCursorScreenPos(ImVec2(viewportImageMin.x + overlayPadding, viewportImageMin.y + overlayPadding));
				if (ImGui::Button(ICON_TOASTER_GLOBE, ImVec2(iconSize, iconSize)))
					mShowPlanetPopup = true;

				ImGui::SetCursorScreenPos(ImVec2(viewportImageMax.x - iconSize - overlayPadding, viewportImageMin.y + overlayPadding));
				if (ImGui::Button(ICON_TOASTER_COG, ImVec2(iconSize, iconSize)))
					mShowSceneSettingsPopup = true;

				ImGui::PopStyleColor(3);
				ImGui::PopFont();
			}

			// Gizmos
			Entity selectedEntity = mSceneHierarchyPanel.GetSelectedEntity();
			if (selectedEntity && mGizmoType != -1 && mSceneState == SceneState::Edit)
			{
				bool entity2D = selectedEntity.HasComponent<UIPanelComponent>() || selectedEntity.HasComponent<UITextComponent>() || selectedEntity.HasComponent<UIButtonComponent>();

				ImGuizmo::SetOrthographic(entity2D);
				float rw = (float)ImGui::GetWindowWidth();
				float rh = (float)ImGui::GetWindowHeight();

				ImGuizmo::SetDrawlist();
				ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, rw, rh);

				// Editor Camera
				DirectX::XMFLOAT4X4 cameraProjection = !entity2D ? mEditorCamera->GetProjection() : mEditorCamera->GetOrthoProjection();
				DirectX::XMFLOAT4X4 cameraView = mEditorCamera->GetViewMatrix();

				// Entity transform	
				auto& tc = selectedEntity.GetComponent<TransformComponent>();

				if (mSceneSettingsPanel.GetSelectionMode() == SceneSettingsPanel::SelectionMode::Entity)
				{
					DirectX::XMFLOAT4X4 transform;
					ImGuizmo::RecomposeMatrixFromComponents(&tc.Translation.x, &tc.RotationEulerAngles.x, &tc.Scale.x, *transform.m);

					// Snapping
					bool snap = Input::IsKeyPressed(Key::LeftControl);
					float snapValue = 0.5f; // Snap to 0.5m degrees for translation/scale
					// Snap to 45 degrees for rotation
					if (mGizmoType == ImGuizmo::OPERATION::ROTATE)
						snapValue = 45.0f;

					float snapValues[3] = { snapValue, snapValue, snapValue };

					ImGuizmo::Manipulate(*cameraView.m, *cameraProjection.m, (ImGuizmo::OPERATION)mGizmoType, ImGuizmo::LOCAL, *transform.m, nullptr, snap ? snapValues : nullptr);

					if (ImGuizmo::IsUsing())
						ImGuizmo::DecomposeMatrixToComponents(*transform.m, &tc.Translation.x, &tc.RotationEulerAngles.x, &tc.Scale.x);
				}
				else
				{
					if (selectedEntity.HasComponent<MeshComponent>())
					{
						auto& mc = selectedEntity.GetComponent<MeshComponent>();

						DirectX::XMMATRIX transformBase = tc.GetTransform() * mc.MeshObject->GetLocalTransform();

						DirectX::XMFLOAT4X4 transform;
						ImGuizmo::RecomposeMatrixFromComponents(&tc.Translation.x, &tc.RotationEulerAngles.x, &tc.Scale.x, *transform.m);

						// Snapping
						bool snap = Input::IsKeyPressed(Key::LeftControl);
						float snapValue = 0.5f; // Snap to 0.5m degrees for translation/scale
						// Snap to 45 degrees for rotation
						if (mGizmoType == ImGuizmo::OPERATION::ROTATE)
							snapValue = 45.0f;

						float snapValues[3] = { snapValue, snapValue, snapValue };

						ImGuizmo::Manipulate(*cameraView.m, *cameraProjection.m, (ImGuizmo::OPERATION)mGizmoType, ImGuizmo::LOCAL, *transform.m, nullptr, snap ? snapValues : nullptr);

						if (ImGuizmo::IsUsing())
						{
							float Ftranslation[3] = { 0.0f, 0.0f, 0.0f }, Frotation[3] = { 0.0f, 0.0f, 0.0f }, Fscale[3] = { 0.0f, 0.0f, 0.0f };
							ImGuizmo::DecomposeMatrixToComponents(*transform.m, Ftranslation, Frotation, Fscale);

							tc.RotationEulerAngles = { Frotation[0], Frotation[1], Frotation[2] };

							mc.MeshObject->SetLocalTransform(DirectX::XMMatrixInverse(nullptr, tc.GetTransform()) * DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(Fscale[0], Fscale[1], Fscale[2])
								* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(Frotation[0]), DirectX::XMConvertToRadians(Frotation[1]), DirectX::XMConvertToRadians(Frotation[2]))))
								* DirectX::XMMatrixTranslation(Ftranslation[0], Ftranslation[1], Ftranslation[2]));
						}
					}
				}
			}

			ImGui::End();
			ImGui::PopStyleVar();

			FrameStats stats;
			if (mEditorScene)
			{
				stats.FPS = mEditorScene->GetFPS();
				stats.VertexCount = mEditorScene->GetVertices();

				if (Scene* active = GetActiveScene())
				{
					entt::entity he = active->GetHoveredEntity();
					if (he != entt::null && active->GetRegistry().valid(he))
					{
						Entity e{ he, active };
						if (e.HasComponent<TagComponent>())
							stats.HoveredEntity = e.GetComponent<TagComponent>().Tag;
					}
				}
			}
			mProfilerPanel.OnImGuiRender(Renderer::GetFrameProfiler(), stats);

			ImGui::End();

			mPreviousViewportSize = mViewportSize;
		}
	}

	void EditorLayer::RenderCustomTitleBar()
	{
		ImGuiIO& io = ImGui::GetIO();

		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		const float barHeight = 40.0f; // The height you want
		ImGui::BeginChild("TitleBar", ImVec2(ImGui::GetContentRegionAvail().x, barHeight),
			false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration);

		// Draw the background rectangle
		ImVec2 barPos = ImGui::GetCursorScreenPos();
		float  width = ImGui::GetContentRegionAvail().x;
		ImVec2 barEnd(barPos.x + width, barPos.y + barHeight);

		ImU32 barColor = ImGui::ColorConvertFloat4ToU32(
			ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive]);
		ImGui::GetWindowDrawList()->AddRectFilled(barPos, barEnd, barColor);

		ImVec4 titleBarColor = ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive];
		ImVec4 titleBarHoveredColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];

		// --- Left Icon ---
		float leftIconSize = 24.0f;
		float leftMargin = 5.0f;
		float topMargin = 3.0f;
		ImGui::PushStyleColor(ImGuiCol_Button, titleBarColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, titleBarHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, titleBarColor);
		ImGui::SetCursorScreenPos(ImVec2(barPos.x + leftMargin, barPos.y + topMargin));
		ImGui::ImageButton("##iconButton", (ImTextureID)(mLogoTex->GetID()), ImVec2(leftIconSize, leftIconSize), ImVec2(0, 0), ImVec2(1, 1));

		// --- Menu Bar (File / Script) ---
		{
			// Use the normal (default) font.
			ImFont* normalFont = io.Fonts->Fonts[0];
			ImGui::PushFont(normalFont);

			// Position the menu area: start at a gap (menuBarMargin) to the right of the left icon.
			float menuBarMargin = 20.0f; // gap between left icon and menu area
			float menuBarX = barPos.x + leftMargin + leftIconSize + menuBarMargin;
			float menuBarY = barPos.y + topMargin + 5.0f; // a slight vertical adjustment
			ImGui::SetCursorScreenPos(ImVec2(menuBarX, menuBarY));

			// Compute the widths of the two menus (adding a bit of padding)
			float fileWidth = ImGui::CalcTextSize("File").x + 10.0f;
			float scriptWidth = ImGui::CalcTextSize("Script").x + 10.0f;
			float planetWidth = ImGui::CalcTextSize("Planet").x + 10.0f;
			// Define a small gap between them (e.g. 5px)
			float gapBetweenMenus = 15.0f;
			// The total width of the menus container
			float menusAreaWidth = fileWidth + scriptWidth + planetWidth + gapBetweenMenus;

			// Create a child container for the menu buttons. We add the MenuBar flag so that
			// the popups open below rather than to the side.
			ImGui::BeginChild("MenusContainer", ImVec2(menusAreaWidth, 0), false,
				ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration);

			// Optionally, push a tighter item spacing for the menu buttons.
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

			if(ImGui::BeginMenuBar())
			{
				// Draw the "File" menu.
				if (ImGui::BeginMenu("File"))
				{
					ImGui::SetNextWindowSizeConstraints(ImVec2(200, 0), ImVec2(FLT_MAX, FLT_MAX));
					if (ImGui::MenuItem("New Project", "Ctrl+Shift+N"))
					{
						mShowProjectPopup = true;
						mProjectPopupMode = ProjectPopupMode::NewProject;
					}
					if (ImGui::MenuItem("Open Project", "Ctrl+O"))
					{
						mShowProjectPopup = true;
						mProjectPopupMode = ProjectPopupMode::OpenProject;
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Save Project", "Ctrl+S"))
						SaveProject();
					if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
						SaveScene();
					if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
						SaveSceneAs();
					ImGui::Separator();
					if (ImGui::MenuItem("Exit"))
						Application::Get().Close();
					ImGui::EndMenu();
				}
				// Place the "Script" menu immediately next to "File" with the small gap.
				ImGui::SameLine(0, gapBetweenMenus);
				if (ImGui::BeginMenu("Script"))
				{
					if (ImGui::MenuItem("Editor", nullptr, mScriptEditorPanel.IsOpen()))
						mScriptEditorPanel.SetOpen(!mScriptEditorPanel.IsOpen());

					if (ImGui::MenuItem("Reload Assembly", "Ctrl+R"))
						ScriptEngine::ReloadAssembly();
					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}

			ImGui::PopStyleVar(); // Pop ItemSpacing override.
			ImGui::EndChild();
			ImGui::PopFont();
		}

		// --- Center Section: Toolbar Icons ---
		// Place the toolbar icons just below the text.
		float iconsTopMargin = 10.0f; // vertical margin between text and icons
		float centerIconSize = 16.0f;
		float centerIconSpacing = 5.0f;
		// Total width for 3 icons.
		float totalIconsWidth = centerIconSize * 3 + centerIconSpacing * 2;
		float iconsX = barPos.x + width * 0.5f - totalIconsWidth * 0.5f;
		float iconsY = barPos.y + iconsTopMargin;
		ImGui::SetCursorScreenPos(ImVec2(iconsX, iconsY));

		// Now inline your ImageButton code (no separate ImGui::Begin/End for "Toolbar"):
		if (mSceneState == SceneState::Edit)
		{
			if (ImGui::ImageButton("##playButton", (ImTextureID)(mPlayButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
			{
				OnScenePlay();
			}
			ImGui::SameLine();
			ImGui::ImageButton("##pauseButton", (ImTextureID)(mPauseButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize));
			ImGui::SameLine();
			ImGui::ImageButton("##stopButton", (ImTextureID)(mStopButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize));
		}
		else if (mSceneState == SceneState::Play)
		{
			ImGui::ImageButton("##playButton", mPlayButtonTex->GetID(), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::SameLine();
			if (ImGui::ImageButton("##pauseButton", (ImTextureID)(mPauseButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
			{
				mRuntimeScene->SetPaused(true);

				OnScenePause();
			}
			ImGui::SameLine();
			if (ImGui::ImageButton("##stopButton", (ImTextureID)(mStopButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
				OnSceneStop();
		}
		else if (mSceneState == SceneState::Pause)
		{
			if (ImGui::ImageButton("##playButton", (ImTextureID)(mPlayButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
			{
				OnSceneUnpause();
				mRuntimeScene->SetPaused(false);
			}
			ImGui::SameLine();
			if (ImGui::ImageButton("##pauseButton", (ImTextureID)(mPauseButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
			ImGui::SameLine();
			if (ImGui::ImageButton("##stopButton", (ImTextureID)(mStopButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
				OnSceneStop();
		}

		// --- Right Side Buttons(Minimize, Maximize, Close) ---
		float rightMargin = 10.0f;
		float rightButtonSpacing = 15.0f;
		float buttonSize = 12.0f;
		float rightButtonsY = barPos.y + 5.0f;

		// Close button (furthest right)
		float closeButtonX = barPos.x + width - rightMargin - buttonSize;
		ImGui::SetCursorScreenPos(ImVec2(closeButtonX, rightButtonsY));
		if (ImGui::ImageButton("##closeButton", (ImTextureID)(mCloseButtonTex->GetID()), ImVec2(buttonSize, buttonSize)))
		{
			Application::Get().Close();
		}
		// Maximize button to the left of Close.
		float maxButtonX = closeButtonX - rightButtonSpacing - buttonSize;
		ImGui::SetCursorScreenPos(ImVec2(maxButtonX, rightButtonsY));
		if (ImGui::ImageButton("##maximizeButton", (ImTextureID)(mMaxButtonTex->GetID()), ImVec2(buttonSize, buttonSize)))
		{
			// Maximize action.
		}
		// Minimize button to the left of Maximize.
		float minButtonX = maxButtonX - rightButtonSpacing - buttonSize;
		ImGui::SetCursorScreenPos(ImVec2(minButtonX, rightButtonsY));
		if (ImGui::ImageButton("##minimizeButton", (ImTextureID)(mMinButtonTex->GetID()), ImVec2(buttonSize, buttonSize)))
		{
			// Minimize action.
		}
		ImGui::PopStyleColor(3);

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
	}

	void EditorLayer::ShowProjectPopup(bool nonForcedPopup)
	{
		ImVec4 titleBarColor = ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive];
		ImVec4 titleBarHoveredColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
		ImVec4 buttonColor = ImGui::GetStyle().Colors[ImGuiCol_Button];

		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, titleBarColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, titleBarHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, titleBarColor);

		ImGui::SetNextWindowBgAlpha(1.0f);
		ImGui::SetNextWindowSize(ImVec2(720, 360), ImGuiCond_FirstUseEver);

		if (ImGui::BeginPopupModal("ProjectPopup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove))
		{
			// ---- Header row (title + close) ----
			const float contentWidth = ImGui::GetWindowContentRegionMax().x;
			const float closeButtonSize = 16.0f;
			const float padding = 10.0f;

			ImGui::Text("Project");

			if (nonForcedPopup)
			{
				float contentWidth = ImGui::GetWindowContentRegionMax().x;
				float closeButtonSize = 16.0f;
				float padding = 10.0f;

				ImGui::SameLine(contentWidth - closeButtonSize - padding);

				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive]);
				if (ImGui::ImageButton("##closeButton", (ImTextureID)(mCloseButtonTex->GetID()),	ImVec2(closeButtonSize, closeButtonSize)))
					ImGui::CloseCurrentPopup();

				ImGui::PopStyleColor();
			}

			ImGui::Separator();

			// ---- Layout constants ----
			const float leftPaneWidth = 160.0f;
			const float outerPadding = 12.0f;

			ImGui::Dummy(ImVec2(0.0f, 6.0f));
			ImGui::Indent(outerPadding);

			// ---- Split: left pane / right pane ----
			ImGui::BeginGroup();

			// Left pane
			ImGui::BeginChild("##ProjectPopupLeft", ImVec2(leftPaneWidth, 0), true);

			ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);

			auto ModeButton = [&](const char* label, ProjectPopupMode mode)
				{
					const bool selected = (mProjectPopupMode == mode);

					// Give selected mode a subtle visual emphasis
					if (selected)
						ImGui::PushStyleColor(ImGuiCol_Button, titleBarHoveredColor);

					const float w = ImGui::GetContentRegionAvail().x;
					if (ImGui::Button(label, ImVec2(w, 0)))
						mProjectPopupMode = mode;

					if (selected)
						ImGui::PopStyleColor();
				};

			ModeButton("New Project", ProjectPopupMode::NewProject);
			ImGui::Spacing();
			ModeButton("Open Project", ProjectPopupMode::OpenProject);

			ImGui::PopStyleColor();
			ImGui::EndChild();

			ImGui::SameLine();

			// Right pane
			ImGui::BeginChild("##ProjectPopupRight", ImVec2(0, 0), true);

			// Right-pane content switches based on mode
			if (mProjectPopupMode == ProjectPopupMode::NewProject)
			{
				ImGui::Text("Create a new project");
				ImGui::Dummy(ImVec2(0.0f, 6.0f));

				ImGui::Text("Project Name");
				ImGui::PushItemWidth(320);
				ImGui::InputText("##projectName", mNewProjectName, sizeof(mNewProjectName));
				ImGui::PopItemWidth();

				ImGui::Dummy(ImVec2(0.0f, 8.0f));

				ImGui::Text("Location");
				ImGui::PushItemWidth(320);
				ImGui::InputText("##newLocation", mNewProjectLocation, sizeof(mNewProjectLocation));
				ImGui::PopItemWidth();

				ImGui::SameLine();
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
				if (ImGui::Button("...", ImVec2(30, 0)))
				{
					std::optional<std::string> folderPath = FileDialogs::OpenFolder("C:\\");
					if (folderPath.has_value())
					{
						strncpy(mNewProjectLocation, folderPath->c_str(), sizeof(mNewProjectLocation));
						mNewProjectLocation[sizeof(mNewProjectLocation) - 1] = '\0';
					}
				}
				ImGui::PopStyleVar();

				ImGui::Dummy(ImVec2(0.0f, 8.0f));
				ImGui::TextDisabled("This will create: <Location>/<Project Name>/");
			}
			else // OpenProject
			{
				ImGui::Text("Open an existing project");
				ImGui::Dummy(ImVec2(0.0f, 6.0f));

				ImGui::Text("Project Folder");
				ImGui::PushItemWidth(320);
				ImGui::InputText("##openProjectPath", mOpenProjectPath, sizeof(mOpenProjectPath));
				ImGui::PopItemWidth();

				ImGui::SameLine();
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
				if (ImGui::Button("...", ImVec2(30, 0)))
				{
					std::optional<std::string> folderPath = FileDialogs::OpenFolder("C:\\");
					if (folderPath.has_value())
					{
						strncpy(mOpenProjectPath, folderPath->c_str(), sizeof(mOpenProjectPath));
						mOpenProjectPath[sizeof(mOpenProjectPath) - 1] = '\0';
					}
				}
				ImGui::PopStyleVar();

				ImGui::Dummy(ImVec2(0.0f, 8.0f));
				ImGui::TextDisabled("Select the folder that contains your project file.");
			}

			// ---- Bottom-right action row inside right pane ----
			{
				// Push cursor down
				const float winHeight = ImGui::GetWindowSize().y;
				const float currentY = ImGui::GetCursorPosY();
				const float buttonHeight = ImGui::GetFrameHeight();
				const float marginBottom = 14.0f;
				float dummyHeight = winHeight - currentY - (buttonHeight + marginBottom);
				if (dummyHeight > 0)
					ImGui::Dummy(ImVec2(0, dummyHeight));

				// Right-align action button
				const float availWidth = ImGui::GetContentRegionAvail().x;
				const float buttonWidth = 90.0f;
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - buttonWidth));

				const char* actionLabel = (mProjectPopupMode == ProjectPopupMode::NewProject) ? "Create" : "Open";

				ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);

				const bool clicked = ImGui::Button(actionLabel, ImVec2(buttonWidth, 0));

				ImGui::PopStyleColor();

				if (clicked)
				{
					try
					{
						if (mProjectPopupMode == ProjectPopupMode::NewProject)
						{
							std::filesystem::path basePath(mNewProjectLocation);
							std::filesystem::path projectPath = basePath / mNewProjectName;

							std::string projectNameStr(mNewProjectName);
							mProject = CreateRef<Project>(projectNameStr, projectPath);

							const std::filesystem::path scenePath =	mProject->GetPath() / mProject->GetActiveScenePath();
							mSceneFilePath = scenePath.string();

							OpenScene(scenePath);

							SetContexts();

							mForceProjectPopup = false;

							ImGui::CloseCurrentPopup();
						}
						else 
						{
							std::filesystem::path folder(mOpenProjectPath);
							if (!std::filesystem::exists(folder))
							{
								TOAST_CORE_WARN("Project folder does not exist.");
							}
							else
							{
								auto projectFileOpt = FindProjectFileInFolder(folder);
								if (!projectFileOpt)
								{
									TOAST_CORE_WARN("No .tproj file found in folder: %s", folder.string().c_str());
								}
								else
								{
									// Create a new empty project instance, then deserialize into it
									auto loadedProject = std::make_shared<Project>();

									ProjectSerializer serializer(loadedProject.get());
									if (!serializer.Deserialize(projectFileOpt->string()))
									{
										TOAST_CORE_ERROR("Failed to load project: %s", projectFileOpt->string().c_str());
									}
									else
									{
										mProject = loadedProject;

										const std::filesystem::path scenePath = mProject->GetPath() / mProject->GetActiveScenePath();
										mSceneFilePath = scenePath.string();

										OpenScene(scenePath);

										SetContexts();

										mForceProjectPopup = false;

										ImGui::CloseCurrentPopup();
									}
								}
							}
						}
					}
					catch (const std::filesystem::filesystem_error&)
					{
						TOAST_CORE_CRITICAL("Something went wrong with creating/opening the project");
					}
				}
			}

			ImGui::EndChild(); // right
			ImGui::EndGroup(); // split group

			ImGui::Unindent(outerPadding);

			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(4);
	}

	void EditorLayer::SetContexts()
	{
		mEditorScene->SetActiveCamera(mEditorCamera);
		mEditorCamera->UpdateView();

		mSceneHierarchyPanel.SetContext(mEditorScene);
		mSceneSettingsPanel.SetContext(mEditorScene, mWindow);
		mEnvironmentPanel.SetContext(mEditorScene);
		mPlanetPanel.SetContext(mEditorScene, mWindow);
		mPropertiesPanel.SetContext(mSceneHierarchyPanel.GetSelectedEntity(), &mSceneHierarchyPanel, mWindow);
		mProjectPanel.SetContext(mProject.get());
	}

	void EditorLayer::OnEvent(Event& e)
	{
		if (mEditorScene)
		{
			if (mSceneState == SceneState::Edit)
				mEditorCamera->OnEvent(e);

			EventDispatcher dispatcher(e);
			dispatcher.Dispatch<KeyPressedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
			dispatcher.Dispatch<MouseButtonPressedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
			dispatcher.Dispatch<MouseButtonReleasedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnMouseButtonReleased));
			dispatcher.Dispatch<MouseMovedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnMouseMoved));
		}
	}

	void EditorLayer::SaveProject()
	{
		if (!mProject)
			return;

		SaveScene();

		std::filesystem::path projectFilePath =	mProject->GetPath() / (mProject->GetName() + ".tproj");

		ProjectSerializer serializer(mProject.get());
		serializer.Serialize(projectFilePath.string());

		AssetManager::SerializeRegistry();
	}

	void EditorLayer::OnScenePlay()
	{
		mSceneState = SceneState::Play;

		mRuntimeScene = CreateRef<Scene>("Runtime");

		mEditorScene->CopyTo(mRuntimeScene.get());

		mRuntimeScene->SetHoveredEntity(entt::null);

		mRuntimeScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		mRuntimeScene->SetViewportPos(mAbsoluteViewportPos);

		mRuntimeScene->OnRuntimeStart();
		mSceneHierarchyPanel.SetContext(mRuntimeScene.get());
	}

	void EditorLayer::OnScenePause()
	{
		if (mSceneState == SceneState::Edit)
			return;

		mSceneState = SceneState::Pause;
	}

	void EditorLayer::OnSceneUnpause()
	{
		if (mSceneState == SceneState::Edit)
			return;

		mSceneState = SceneState::Play;
	}

	void EditorLayer::OnSceneStop()
	{
		mRuntimeScene->OnRuntimeStop();
		mSceneState = SceneState::Edit;

		mSceneHierarchyPanel.SetContext(mEditorScene);
		mEditorScene->InvalidateFrustum();
	}

	void EditorLayer::OpenProjectScene(UUID id)
	{
		if (!mProject)
			return;

		const auto relPath = mProject->GetScenePath(id);
		if (relPath.empty())
			return;

		const std::filesystem::path absPath = mProject->GetPath() / relPath;

		mProject->SetActiveScene(id);

		mSceneFilePath = absPath.string();
		UpdateWindowTitle(absPath.filename().string());

		OpenScene(absPath); 

		SetContexts();
	}

	Scene* EditorLayer::GetActiveScene()
	{
		switch (mSceneState)
		{
		case SceneState::Play:
		case SceneState::Pause:
			return mRuntimeScene ? mRuntimeScene.get() : mEditorScene;
		case SceneState::Edit:
			return mEditorScene;
		default:
			return mEditorScene;
		}
	}

	void EditorLayer::OpenScene()
	{
		std::optional<std::string> filepath = FileDialogs::OpenFile("Toast Scene(*.toast)\0*toast\0", "..\\Toaster\\assets\\scenes\\");
		if (filepath)
		{
			std::filesystem::path path = *filepath;
			UpdateWindowTitle(path.filename().string());
			mSceneFilePath = *filepath;
			OpenScene(path);
		}

		mEditorScene->SetActiveCamera(mEditorCamera);
		mEditorCamera->UpdateView();
	}

	void EditorLayer::OpenScene(const std::filesystem::path& path)
	{
		ResetEditorScene();

		SceneSerializer serializer(mEditorScene);
		if (!serializer.Deserialize(path.string(), mEditorCamera.get()))
		{
			TOAST_CORE_ERROR("Failed to open scene: %s", path.string().c_str());
			return;
		}
	}

	void EditorLayer::ResetEditorScene()
	{
		// If you’re currently playing, stop first (runtime scene may be referencing old data)
		if (mSceneState != SceneState::Edit)
			OnSceneStop();

		// Create a fresh scene instance
		mPlaceholderScene = CreateScope<Scene>();
		mEditorScene = mPlaceholderScene.get();

		// Re-bind camera
		mEditorScene->SetActiveCamera(mEditorCamera);
		mEditorCamera->UpdateView();
	}

	void EditorLayer::SwitchRuntimeToProjectScene(UUID id)
	{
		if (!mProject)
			return;

		const auto relPath = mProject->GetScenePath(id);
		if (relPath.empty())
		{
			TOAST_CORE_WARN("SwitchRuntimeToProjectScene: scene path missing for id.");
			return;
		}

		const std::filesystem::path absPath = mProject->GetPath() / relPath;

		// Stop current runtime scene cleanly
		if (mRuntimeScene)
			mRuntimeScene->OnRuntimeStop();

		mHoveredEntity = Entity{};
		mSceneHierarchyPanel.SetSelectedEntity({}); // if you have this API
		if (mRuntimeScene)
			mRuntimeScene->SetHoveredEntity(entt::null);

		// Keep play state (don’t drop back to edit)
		mSceneState = SceneState::Play;

		// Create a fresh runtime scene and deserialize into it
		mRuntimeScene = CreateRef<Scene>();

		SceneSerializer serializer(mRuntimeScene.get());
		if (!serializer.Deserialize(absPath.string(), /*editorCamera*/ nullptr))
		{
			TOAST_CORE_ERROR("Failed to open runtime scene: %s", absPath.string().c_str());
			// Fallback: go back to edit (optional)
			mSceneState = SceneState::Edit;
			mSceneHierarchyPanel.SetContext(mEditorScene);
			return;
		}

		mHoveredEntity = Entity{};
		mRuntimeScene->SetHoveredEntity(entt::null);

		// Ensure runtime viewport settings continue to work
		mRuntimeScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		mRuntimeScene->SetViewportPos(mAbsoluteViewportPos);

		// Update project active scene
		mProject->SetActiveScene(id);

		// Update panels to point at runtime context
		mSceneHierarchyPanel.SetContext(mRuntimeScene.get());
		mRuntimeScene->SetHoveredEntity(entt::null);

		mBlockRuntimeFrames = 20;

		// Start runtime
		mRuntimeScene->OnRuntimeStart();

		TOAST_CORE_INFO("Runtime switched to scene: %s", absPath.filename().string().c_str());
	}

	void EditorLayer::SaveScene()
	{
		if (!mProject)
			return;

		const UUID activeID = mProject->GetActiveSceneID();

		// Resolve the scene path from the project
		const std::filesystem::path rel = mProject->GetScenePath(activeID);
		if (rel.empty())
		{
			// Project doesn't know where this scene should be saved yet
			SaveSceneAs();
			return;
		}

		const std::filesystem::path abs = mProject->GetPath() / rel;

		// Ensure destination directory exists
		std::filesystem::create_directories(abs.parent_path());

		// Use the project's display name (filename stem) as the scene name in YAML
		const std::string sceneName = mProject->GetSceneDisplayName(activeID);

		// Serialize the editor scene (or whichever scene instance you are editing)
		SceneSerializer serializer(mEditorScene);
		serializer.Serialize(abs.string(), sceneName, mEditorCamera.get());
	}

	void EditorLayer::SaveSceneAs()
	{
		mSceneFilePath = FileDialogs::SaveFile("Toast Scene(*.tscene)\0*tscene\0");
		if (mSceneFilePath)
		{
			SceneSerializer serializer(mEditorScene);
			serializer.Serialize(*mSceneFilePath, "Untitled Scene", mEditorCamera.get());

			std::filesystem::path path = *mSceneFilePath;
			UpdateWindowTitle(path.filename().string());
		}
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		switch (e.GetKeyCode())
		{
		case Key::O:
		{
			if (control)
				OpenScene();
			break;
		}
		case Key::S:
		{
			if (control && shift)
				SaveSceneAs();
			else if (control && !shift)
				SaveScene();
			break;
		}

		//Gizmos
		case Key::Q:
		{
			if (!ImGuizmo::IsUsing())
				mGizmoType = -1;

			break;
		}
		case Key::W:
		{
			if (!ImGuizmo::IsUsing())
				mGizmoType = ImGuizmo::OPERATION::TRANSLATE;

			break;
		}
		case Key::E:
		{
			if (!ImGuizmo::IsUsing())
				mGizmoType = ImGuizmo::OPERATION::ROTATE;

			break;
		}
		case Key::R:
		{
			if (control)
				ScriptEngine::ReloadAssembly();
			else 
			{
				if (!ImGuizmo::IsUsing())
					mGizmoType = ImGuizmo::OPERATION::SCALE;
			}
			break;
		}
		}

		return true;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{
		if (e.GetMouseButton() == Mouse::ButtonLeft)
		{
			if (mViewportHovered && !ImGuizmo::IsOver() && !Input::IsKeyPressed(Key::LeftAlt) && mSceneState == SceneState::Edit) 
			{
				mSceneHierarchyPanel.SetSelectedEntity(mHoveredEntity);
				mEditorScene->SetSelectedEntity(mHoveredEntity);

				if (mHoveredEntity)
				{
					DirectX::XMFLOAT3 focalPoint = mHoveredEntity.GetComponent<TransformComponent>().Translation;
					mEditorCamera->UpdateFocalPoint(DirectX::XMLoadFloat3(&focalPoint));
				}
			}

			ImGuiIO& io = ImGui::GetIO();
			io.MouseDown[0] = true;
		}

		return true;
	}

	bool EditorLayer::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
	{
		if (e.GetMouseButton() == Mouse::ButtonLeft)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MouseDown[0] = false;
		}

		return true;
	}

	bool EditorLayer::OnMouseMoved(MouseMovedEvent& e)
	{
		if (mSceneState == SceneState::Edit)
		{
			entt::entity sceneHovered = mEditorScene->GetHoveredEntity();

			mHoveredEntity = (sceneHovered == entt::null) ? Entity() : Entity(sceneHovered, mEditorScene);
		}
		else if (mSceneState == SceneState::Play)
		{
			entt::entity sceneHovered = mRuntimeScene->GetHoveredEntity();

			mHoveredEntity = (sceneHovered == entt::null) ? Entity() : Entity(sceneHovered, mRuntimeScene.get());
		}

		return true;
	}

	void EditorLayer::UpdateWindowTitle(const std::string& sceneName)
	{
		std::string title = sceneName + " - Toaster - " + Application::GetPlatformName() + " (" + Application::GetConfigurationName() + ")";
		//Application::Get().GetWindow().SetTitle(title);
	}

	void EditorLayer::UpdateWindowIcon(const std::string& iconPath)
	{
		Application::Get().GetWindow().SetIcon(iconPath);
	}

	void EditorLayer::RequestSceneChange(const std::string& sceneName)
	{
		// Scene switch queued for next frame
		mPendingSceneChangeName = sceneName;
	}

}