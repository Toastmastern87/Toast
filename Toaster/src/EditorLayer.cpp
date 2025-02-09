#include "EditorLayer.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

#include "Toast/Core/Application.h"
#include "Toast/Core/Input.h"

#include "Toast/Scene/SceneSerializer.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Utils/PlatformUtils.h"

#include "imgui/imgui_internal.h"
#include "imgui/imgui.h"

#include <filesystem>

#include "FontAwesome.h"

#include "ImGuizmo.h"

namespace Toast {
	
	extern const std::filesystem::path gAssetPath;

	EditorLayer::EditorLayer(WindowsWindow* window)
		: Layer("TheNextFrontier2D", window)
	{
		mWindow = window;
	}

	void EditorLayer::OnAttach()
	{
		TOAST_PROFILE_FUNCTION();

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

		// Samplers
		TextureLibrary::LoadTextureSampler("Default", D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);
		TextureLibrary::LoadTextureSampler("LinearSampler", D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
		TextureLibrary::LoadTextureSampler("PointSampler", D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
		TextureLibrary::LoadTextureSampler("BRDFSampler", D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);

		// Load all shaders
		// Deffered Rendering
		ShaderLibrary::Load("assets/shaders/Deffered Rendering/GeometryPass.hlsl");
		ShaderLibrary::Load("assets/shaders/Deffered Rendering/ShadowPass.hlsl");
		ShaderLibrary::Load("assets/shaders/Deffered Rendering/SSAOPass.hlsl");
		ShaderLibrary::Load("assets/shaders/Deffered Rendering/LightningPass.hlsl");
		ShaderLibrary::Load("assets/shaders/Debug/ObjectMask.hlsl");

		// Post Processes
		ShaderLibrary::Load("assets/shaders/Post Process/Skybox.hlsl");
		ShaderLibrary::Load("assets/shaders/Post Process/Atmosphere.hlsl");
		ShaderLibrary::Load("assets/shaders/Post Process/ToneMapping.hlsl");

		// Environment
		ShaderLibrary::Load("assets/shaders/Environment/EnvironmentMipFilter.hlsl");
		ShaderLibrary::Load("assets/shaders/Environment/EnvironmentIrradiance.hlsl");

		// Others
		ShaderLibrary::Load("assets/shaders/Standard.hlsl");
		ShaderLibrary::Load("assets/shaders/UI.hlsl");

		// Load all materials from the asset folder
		std::vector<std::string> materialStrings = FileDialogs::GetAllFiles("\\assets\\materials");
		MaterialSerializer::Deserialize(materialStrings);

		NewScene();

		mEditorCamera = CreateRef<EditorCamera>(30.0f, 1.778f, 0.1f, 1000000.0f);

		mSceneHierarchyPanel.SetContext(mEditorScene);
		mSceneSettingsPanel.SetContext(mEditorScene, mWindow);
		mEnvironmentPanel.SetContext(mEditorScene);
		mPropertiesPanel.SetContext(mSceneHierarchyPanel.GetSelectedEntity(), &mSceneHierarchyPanel, mWindow);
	}

	void EditorLayer::OnDetach()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		TOAST_PROFILE_FUNCTION();

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
			if(mViewportHovered)
				mEditorCamera->OnUpdate(ts);

			mEditorScene->OnUpdateEditor(ts, mEditorCamera);
			mEditorScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

			break;
		}
		case SceneState::Play:
		{
			mRuntimeScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			mRuntimeScene->SetViewportPos(mAbsoluteViewportPos);
			mRuntimeScene->OnUpdateRuntime(ts);

			break;
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
			ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
			ImGui::PopStyleVar();

			RenderCustomTitleBar();

			if (mShowNewProjectPopup)
			{
				// Call OpenPopup *after* the menu has been closed.
				ImGui::OpenPopup("NewProjectPopup");
				mShowNewProjectPopup = false;
			}

			ShowCreateNewProject();

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

			ImGui::BeginChild("DockSpaceRegion", ImVec2(0, 0), false,
				ImGuiWindowFlags_NoScrollbar 
				| ImGuiWindowFlags_NoScrollWithMouse 
				| ImGuiWindowFlags_NoDecoration);

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

			mSceneSettingsPanel.OnImGuiRender(mActiveDragArea);
			mSceneHierarchyPanel.OnImGuiRender();
			mMaterialPanel.OnImGuiRender();
			mEnvironmentPanel.OnImGuiRender();
			mContentBrowserPanel.OnImGuiRender();
			mConsolePanel.OnImGuiRender();
			mPropertiesPanel.OnImGuiRender(mActiveDragArea);

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

			if(mViewportSize.x != mPreviousViewportSize.x || mViewportSize.y != mPreviousViewportSize.y)
				Renderer::OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

			//TOAST_CORE_INFO("Setting viewport size to mViewportSize.x: %f, mViewportSize.y: %f", mViewportSize.x, mViewportSize.y);

			void* textureID = nullptr;

			switch (mEditorScene->GetSettings().RenderOverlaySetting)
			{
			case RenderOverlay::NONE:
				textureID = (void*)Renderer::GetFinalRT()->GetSRV().Get();
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
			}

			Ref<RenderTarget>& finalRenderTarget = Renderer::GetFinalRT();
			ImGui::Image(textureID, ImVec2{ mViewportSize.x, mViewportSize.y });

			if (ImGui::BeginDragDropTarget()) 
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					OpenScene(std::filesystem::path(gAssetPath) / path);
				}

				ImGui::EndDragDropTarget();
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

			ImGui::Begin(ICON_TOASTER_CALCULATOR" Statistics");

			std::string name = "none";
			if (mHoveredEntity)
				name = mHoveredEntity.GetComponent<TagComponent>().Tag;
			ImGui::Text("Hovered Entity: %s", name.c_str());

			ImGui::Text("FPS: %d", mEditorScene->GetFPS());
			ImGui::Text("Frame time: %fms", mEditorScene->GetFrameTime());
			ImGui::Text("Vertex count: %d", mEditorScene->GetVertices());

			ImGui::End();

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

		// For example, use the same color as TitleBgActive:
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
		ImGui::ImageButton((ImTextureID)(mLogoTex->GetID()), ImVec2(leftIconSize, leftIconSize), ImVec2(0, 0), ImVec2(1, 1));

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
			// Define a small gap between them (e.g. 5px)
			float gapBetweenMenus = 15.0f;
			// The total width of the menus container
			float menusAreaWidth = fileWidth + scriptWidth + gapBetweenMenus;

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
					if (ImGui::BeginMenu("New"))
					{
						if (ImGui::MenuItem("Project", "Ctrl+Shift+N"))
							mShowNewProjectPopup = true;

						if (ImGui::MenuItem("Scene", "Ctrl+N"))
							NewScene();

						ImGui::EndMenu();
					}

					if (ImGui::MenuItem("Open...", "Ctrl+O"))
						OpenScene();
					ImGui::Separator();
					if (ImGui::MenuItem("Save", "Ctrl+S"))
						SaveScene();
					if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
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
			if (ImGui::ImageButton((ImTextureID)(mPlayButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1),
				-1, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
			{
				OnScenePlay();
			}
			ImGui::SameLine();
			ImGui::ImageButton((ImTextureID)(mPauseButtonTex->GetID()),
				ImVec2(centerIconSize, centerIconSize));
			ImGui::SameLine();
			ImGui::ImageButton((ImTextureID)(mStopButtonTex->GetID()),
				ImVec2(centerIconSize, centerIconSize));
		}
		else if (mSceneState == SceneState::Play)
		{
			ImGui::ImageButton((ImTextureID)(mPlayButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::SameLine();
			if (ImGui::ImageButton((ImTextureID)(mPauseButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
			{
				mRuntimeScene->SetPaused(true);

				OnScenePause();
			}
			ImGui::SameLine();
			if (ImGui::ImageButton((ImTextureID)(mStopButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
				OnSceneStop();
		}
		else if (mSceneState == SceneState::Pause)
		{
			if (ImGui::ImageButton((ImTextureID)(mPlayButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
			{
				OnSceneUnpause();
				mRuntimeScene->SetPaused(false);
			}
			ImGui::SameLine();
			if (ImGui::ImageButton((ImTextureID)(mPauseButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
			ImGui::SameLine();
			if (ImGui::ImageButton((ImTextureID)(mStopButtonTex->GetID()), ImVec2(centerIconSize, centerIconSize), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
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
		if (ImGui::ImageButton((ImTextureID)(mCloseButtonTex->GetID()), ImVec2(buttonSize, buttonSize)))
		{
			Application::Get().Close();
		}
		// Maximize button to the left of Close.
		float maxButtonX = closeButtonX - rightButtonSpacing - buttonSize;
		ImGui::SetCursorScreenPos(ImVec2(maxButtonX, rightButtonsY));
		if (ImGui::ImageButton((ImTextureID)(mMaxButtonTex->GetID()), ImVec2(buttonSize, buttonSize)))
		{
			// Maximize action.
		}
		// Minimize button to the left of Maximize.
		float minButtonX = maxButtonX - rightButtonSpacing - buttonSize;
		ImGui::SetCursorScreenPos(ImVec2(minButtonX, rightButtonsY));
		if (ImGui::ImageButton((ImTextureID)(mMinButtonTex->GetID()), ImVec2(buttonSize, buttonSize)))
		{
			// Minimize action.
		}
		ImGui::PopStyleColor(3);

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
	}

	void EditorLayer::ShowCreateNewProject()
	{
		ImVec4 titleBarColor = ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive];
		ImVec4 titleBarHoveredColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
		ImVec4 buttonColor = ImGui::GetStyle().Colors[ImGuiCol_Button];

		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, titleBarColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, titleBarHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, titleBarColor);

		ImGui::SetNextWindowBgAlpha(1.0f);

		ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
		if (ImGui::BeginPopupModal("NewProjectPopup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove ))
		{
			// Calculate positioning for the close button
			float contentWidth = ImGui::GetWindowContentRegionMax().x;
			float closeButtonSize = 16.0f;  // Adjust as needed
			float padding = 10.0f;  // Padding from the right edge

			// Display header text
			ImGui::Text("Create New Project");
			// Position the close button on the same line at the right
			ImGui::SameLine(contentWidth - closeButtonSize - padding);

			{
				// Push a style override for the close button.
				ImGui::PushStyleColor(ImGuiCol_Button, titleBarColor);
				if (ImGui::ImageButton((ImTextureID)(mCloseButtonTex->GetID()), ImVec2(closeButtonSize, closeButtonSize)))
					ImGui::CloseCurrentPopup();

				ImGui::PopStyleColor();
			}

			ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);

			ImGui::Separator();
			
			ImGui::Spacing();  // A few pixels vertical margin.
			ImGui::Text("Project Name");
			ImGui::PushItemWidth(250);
			static char projectName[256] = "";
			ImGui::InputText("##projectName", projectName, sizeof(projectName));
			ImGui::PopItemWidth();

			ImGui::Spacing();  // A few pixels vertical margin.
			ImGui::Text("Location");
			ImGui::PushItemWidth(250);
			static char location[256] = "";
			ImGui::InputText("##Location", location, sizeof(location));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2)); // Small padding for the button.
			if (ImGui::Button("...", ImVec2(30, 0)))
			{
				std::optional<std::string> folderPath = FileDialogs::OpenFolder("C:\\");
				if (folderPath.has_value())
				{
					// Copy the folder path into the 'location' buffer.
					strncpy(location, folderPath->c_str(), sizeof(location));
					location[sizeof(location) - 1] = '\0';
				}
			}
			ImGui::PopStyleVar();

			// --- Push "Create" button to the bottom right ---
			{
				// Get the available vertical space.
				float winHeight = ImGui::GetWindowSize().y;
				float currentY = ImGui::GetCursorPosY();
				float buttonHeight = ImGui::GetFrameHeight();
				float marginBottom = 20.0f;
				float dummyHeight = winHeight - currentY - (buttonHeight + marginBottom);
				if (dummyHeight > 0)
					ImGui::Dummy(ImVec2(0, dummyHeight));

				// Align the button to the right.
				float availWidth = ImGui::GetWindowContentRegionMax().x;
				float buttonWidth = 80.0f;
				float marginRight = 10.0f;
				ImGui::SetCursorPosX(availWidth - buttonWidth - marginRight);
				if (ImGui::Button("Create", ImVec2(buttonWidth, 0)))
				{
					try
					{
						std::filesystem::path basePath(location);

						std::filesystem::path projectPath = basePath / projectName;
						std::filesystem::create_directories(projectPath);

						std::string projectNameStr(projectName);
						mProject = CreateRef<Project>(projectNameStr, projectPath);

						std::filesystem::path assetsPath = projectPath / "Assets";
						std::filesystem::create_directories(assetsPath);

						// Create sub folders inside the "Assets" folder.
						std::filesystem::create_directories(assetsPath / "Scenes");
						std::filesystem::create_directories(assetsPath / "Textures");
						std::filesystem::create_directories(assetsPath / "Fonts");
						std::filesystem::create_directories(assetsPath / "Meshes");
						std::filesystem::create_directories(assetsPath / "Scripts");
						std::filesystem::create_directories(assetsPath / "Materials");

						ImGui::CloseCurrentPopup();
					}
					catch (const std::filesystem::filesystem_error& e)
					{
						TOAST_CORE_CRITICAL("Something went wrong with creating the projet");
					}
				}
			}

			ImGui::PopStyleColor();

			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(4);
	}

	void EditorLayer::OnEvent(Event& e)
	{
		if (mSceneState == SceneState::Edit)
			mEditorCamera->OnEvent(e);
		else if (mSceneState == SceneState::Play)
			mRuntimeScene->OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
		dispatcher.Dispatch<MouseButtonReleasedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnMouseButtonReleased));
		dispatcher.Dispatch<MouseMovedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnMouseMoved));
	}

	void EditorLayer::OnScenePlay()
	{
		mSceneState = SceneState::Play;

		mRuntimeScene = CreateRef<Scene>();
		mEditorScene->CopyTo(mRuntimeScene);

		mRuntimeScene->OnRuntimeStart();
		mSceneHierarchyPanel.SetContext(mRuntimeScene);
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

		mRuntimeScene = nullptr;

		mSceneHierarchyPanel.SetContext(mEditorScene);
		mEditorScene->InvalidateFrustum();
	}

	void EditorLayer::NewScene()
	{
		if (mSceneState != SceneState::Edit)
			return;

		mEditorScene = CreateRef<Scene>();
		//mEditorScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		mSceneHierarchyPanel.SetContext(mEditorScene);
		mEnvironmentPanel.SetContext(mEditorScene);
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
	}

	void EditorLayer::OpenScene(const std::filesystem::path& path)
	{
		mEditorScene = CreateRef<Scene>();
		mEditorScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		mSceneHierarchyPanel.SetContext(mEditorScene);
		mSceneSettingsPanel.SetContext(mEditorScene, mWindow);
		mEnvironmentPanel.SetContext(mEditorScene);

		SceneSerializer serializer(mEditorScene);
		serializer.Deserialize(path.string());
	}

	void EditorLayer::SaveScene()
	{
		if (mSceneFilePath) {
			SceneSerializer serializer(mEditorScene);
			serializer.Serialize(*mSceneFilePath);
		}
		else {
			SaveSceneAs();
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		mSceneFilePath = FileDialogs::SaveFile("Toast Scene(*.toast)\0*toast\0");
		if (mSceneFilePath)
		{
			SceneSerializer serializer(mEditorScene);
			serializer.Serialize(*mSceneFilePath);

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
		case Key::N:
		{
			if (control)
				NewScene();
			break;
		}
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
			{
				ScriptEngine::ReloadAssembly();
			}
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
		Ref<RenderTarget>& pickingRT = Renderer::GetGPassPickingRT();

		auto [mx, my] = ImGui::GetMousePos();
		mx -= mViewportBounds[0].x;		my -= mViewportBounds[0].y;
		DirectX::XMFLOAT2 viewportSize = { mViewportBounds[1].x - mViewportBounds[0].x,  mViewportBounds[1].y - mViewportBounds[0].y };

		int mouseX = (int)mx;
		int mouseY = (int)my;

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
		{
			int pixelData = pickingRT->ReadPixel(mouseX, mouseY);
			mHoveredEntity = pixelData == 0 ? Entity() : Entity((entt::entity)(pixelData - 1), mEditorScene.get());
		}

		if (mViewportHovered && !ImGuizmo::IsOver())
		{
			if (mSceneState == SceneState::Edit)
				mEditorScene->SetHoveredEntity(mHoveredEntity);
			else if (mSceneState == SceneState::Play)
				mRuntimeScene->SetHoveredEntity(mHoveredEntity);
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

}