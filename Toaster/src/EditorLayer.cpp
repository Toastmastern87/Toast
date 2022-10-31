#include "EditorLayer.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

#include "Toast/Core/Application.h"
#include "Toast/Core/Input.h"

#include "Toast/Scene/SceneSerializer.h"

#include "Toast/Script/ScriptEngine.h"

#include "Toast/Utils/PlatformUtils.h"

#include "imgui/imgui_internal.h"
#include "imgui/imgui.h"

#include <filesystem>

#include "FontAwesome.h"

#include "ImGuizmo.h"

namespace Toast {
	
	extern const std::filesystem::path gAssetPath;

	EditorLayer::EditorLayer()
		: Layer("TheNextFrontier2D")
	{
	}

	void EditorLayer::OnAttach()
	{
		TOAST_PROFILE_FUNCTION();

		mCheckerboardTexture = TextureLibrary::LoadTexture2D("assets/textures/Checkerboard.png");
		mPlayButtonTex = TextureLibrary::LoadTexture2D("assets/textures/PlayButton.png");
		mPauseButtonTex = TextureLibrary::LoadTexture2D("assets/textures/PauseButton.png");
		mStopButtonTex = TextureLibrary::LoadTexture2D("assets/textures/StopButton.png");
		mLogoTex = TextureLibrary::LoadTexture2D("assets/textures/ToastEnginePlanetLogo.png");
		TextureLibrary::LoadTexture2D("assets/textures/White.png");
		TextureLibrary::LoadTextureSampler("Default", D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);

		// Load all material shaders
		ShaderLibrary::Load("assets/shaders/Standard.hlsl");
		ShaderLibrary::Load("assets/shaders/Planet.hlsl");
		ShaderLibrary::Load("assets/shaders/ToastPBR.hlsl");
		ShaderLibrary::Load("assets/shaders/Picking.hlsl");
		ShaderLibrary::Load("assets/shaders/ToneMapping.hlsl");
		ShaderLibrary::Load("assets/shaders/Planet/Atmosphere.hlsl");
		ShaderLibrary::Load("assets/shaders/Planet/PlanetMask.hlsl");

		// Load all materials from the computer
		std::vector<std::string> materialStrings = FileDialogs::GetAllFiles("\\assets\\materials");
		MaterialSerializer::Deserialize(materialStrings);

		mEditorScene = CreateRef<Scene>();

		mEditorCamera = CreateRef<EditorCamera>(30.0f, 1.778f, 0.1f, 20000.0f);

		mSceneHierarchyPanel.SetContext(mEditorScene);
		mSceneSettingsPanel.SetContext(mEditorScene);
		mEnvironmentPanel.SetContext(mEditorScene);
		mMaterialPanel.SetContext(MaterialLibrary::Get("Standard"));
		mPropertiesPanel.SetContext(mSceneHierarchyPanel.GetSelectedEntity(), &mSceneHierarchyPanel);
	}

	void EditorLayer::OnDetach()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		TOAST_PROFILE_FUNCTION();
		// Resize
		Ref<Framebuffer>& baseFramebuffer = Renderer::GetBaseFramebuffer();
		Ref<Framebuffer>& finalFramebuffer = Renderer::GetFinalFramebuffer();
		Ref<Framebuffer>& pickingFramebuffer = Renderer::GetPickingFramebuffer();
		Ref<Framebuffer>& outlineFramebuffer = Renderer::GetOutlineFramebuffer();
		Ref<Framebuffer>& planetMaskFramebuffer = Renderer::GetPlanetMaskFramebuffer();
		Ref<Framebuffer>& postProcessFramebuffer = Renderer::GetPostProcessFramebuffer();
		Ref<Framebuffer>& UIFramebuffer = Renderer::GetUIFramebuffer();

		Ref<RenderTarget>& baseRenderTarget = Renderer::GetBaseRenderTarget();
		Ref<RenderTarget>& depthRenderTarget = Renderer::GetDepthRenderTarget();
		Ref<RenderTarget>& finalRenderTarget = Renderer::GetFinalRenderTarget();
		Ref<RenderTarget>& pickingRenderTarget = Renderer::GetPickingRenderTarget();
		Ref<RenderTarget>& outlineRenderTarget = Renderer::GetOutlineRenderTarget();
		Ref<RenderTarget>& planetMaskRenderTarget = Renderer::GetPlanetMaskRenderTarget();
		Ref<RenderTarget>& postProcessRenderTarget = Renderer::GetPostProcessRenderTarget();
		Ref<RenderTarget>& UIRenderTarget = Renderer::GetUIRenderTarget();
		auto [width, height] = baseRenderTarget->GetSize();

		if (mViewportSize.x > 0.0f && mViewportSize.y > 0.0f && (width != mViewportSize.x || height != mViewportSize.y))
		{ 
			baseFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			finalFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			pickingFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			outlineFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			planetMaskFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			postProcessFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			UIFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

			baseRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			depthRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			finalRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			pickingRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			outlineRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			planetMaskRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			postProcessRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			UIRenderTarget->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		 
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

			auto [mx, my] = ImGui::GetMousePos();
			mx -= mViewportBounds[0].x;
			my -= mViewportBounds[0].y;
			DirectX::XMFLOAT2 viewportSize = { mViewportBounds[1].x - mViewportBounds[0].x,  mViewportBounds[1].y - mViewportBounds[0].y };

			int mouseX = (int)mx;
			int mouseY = (int)my;

			if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
			{
				int pixelData = pickingFramebuffer->ReadPixel(mouseX, mouseY);
				mHoveredEntity = pixelData == 0 ? Entity() : Entity((entt::entity)(pixelData - 1), mEditorScene.get());
			}

			break;
		}
		case SceneState::Play:
		{
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
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
			if (opt_fullscreen)
			{
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->Pos);
				ImGui::SetNextWindowSize(viewport->Size);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
				window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
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

			if (opt_fullscreen)
				ImGui::PopStyleVar(2);

			// DockSpace
			ImGuiIO& io = ImGui::GetIO();
			ImGuiStyle& style = ImGui::GetStyle();
			float minWinSize = style.WindowMinSize.x;
			style.WindowMinSize.x = 370.0f;
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

			style.WindowMinSize.x = minWinSize;

			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					// Disabling fullscreen would allow the window to be moved to the front of other windows, 
					// which we can't undo at the moment without finer window depth/z control.
					//ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen_persistant);
					if (ImGui::MenuItem("New", "Ctrl+N"))
						NewScene();

					if (ImGui::MenuItem("Open...", "Ctrl+O"))
						OpenScene();

					ImGui::Separator();
					if (ImGui::MenuItem("Save", "Ctrl+S"))
						SaveScene();
					if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
					{
						SaveSceneAs();
					}

					ImGui::Separator();
					if (ImGui::MenuItem("Exit"))
						Toast::Application::Get().Close();

					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}

			ImGui::Begin("Toolbar", false, ImGuiWindowFlags_NoDecoration);
			ImGui::SetCursorPosX(static_cast<float>(ImGui::GetWindowWidth() / 2.0f));
			if (mSceneState == SceneState::Edit)
			{
				if (ImGui::ImageButton((ImTextureID)(mPlayButtonTex->GetID()), ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
					OnScenePlay();

				ImGui::SameLine();
				ImGui::ImageButton((ImTextureID)(mPauseButtonTex->GetID()), ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::SameLine();
				ImGui::ImageButton((ImTextureID)(mStopButtonTex->GetID()), ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			}
			else if (mSceneState == SceneState::Play)
			{
				ImGui::ImageButton((ImTextureID)(mPlayButtonTex->GetID()), ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::SameLine();
				ImGui::ImageButton((ImTextureID)(mPauseButtonTex->GetID()), ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				ImGui::SameLine();
				if (ImGui::ImageButton((ImTextureID)(mStopButtonTex->GetID()), ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)))
					OnSceneStop();
			}
			ImGui::SameLine();
			ImGui::End();

			mSceneSettingsPanel.OnImGuiRender();
			mSceneHierarchyPanel.OnImGuiRender();
			mMaterialPanel.OnImGuiRender();
			mEnvironmentPanel.OnImGuiRender();
			mConsolePanel.OnImGuiRender();
			mContentBrowserPanel.OnImGuiRender();
			mPropertiesPanel.OnImGuiRender();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
			ImGui::Begin(ICON_TOASTER_GAMEPAD" Viewport");
			auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
			auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
			auto viewportOffset = ImGui::GetWindowPos();

			mViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
			mViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

			mViewportFocused = ImGui::IsWindowFocused();
			mViewportHovered = ImGui::IsWindowHovered();
			Application::Get().GetImGuiLayer()->BlockEvents(!mViewportFocused && !mViewportHovered);

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
			mViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

			Ref<RenderTarget>& finalRenderTarget = Renderer::GetFinalRenderTarget();
			ImGui::Image((void*)finalRenderTarget->GetSRV().Get(), ImVec2{ mViewportSize.x, mViewportSize.y });

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
				ImGuizmo::SetOrthographic(false);
				ImGuizmo::SetDrawlist();
				float windowWidth = (float)ImGui::GetWindowWidth();
				float windowHeight = (float)ImGui::GetWindowHeight();
				ImGuizmo::SetRect(mViewportBounds[0].x, mViewportBounds[0].y, mViewportBounds[1].x - mViewportBounds[0].x, mViewportBounds[1].y - mViewportBounds[0].y);

				// Editor Camera
				DirectX::XMFLOAT4X4 cameraProjection = mEditorCamera->GetProjection();
				DirectX::XMFLOAT4X4 cameraView = mEditorCamera->GetViewMatrix();

				// Entity transform
				DirectX::XMFLOAT3 translationFloat3, scaleFloat3;
				DirectX::XMVECTOR translation, scale, rotation;

				auto& tc = selectedEntity.GetComponent<TransformComponent>();

				if (mSceneSettingsPanel.GetSelectionMode() == SceneSettingsPanel::SelectionMode::Entity)
				{
					DirectX::XMMatrixDecompose(&scale, &rotation, &translation, tc.Transform);
					DirectX::XMStoreFloat3(&translationFloat3, translation);
					DirectX::XMStoreFloat3(&scaleFloat3, scale);
					DirectX::XMFLOAT4X4 transform;
					float Ftranslation[3] = { translationFloat3.x, translationFloat3.y, translationFloat3.z };
					float Frotation[3] = { tc.RotationEulerAngles.x, tc.RotationEulerAngles.y, tc.RotationEulerAngles.z };
					float Fscale[3] = { scaleFloat3.x, scaleFloat3.y, scaleFloat3.z };
					ImGuizmo::RecomposeMatrixFromComponents(Ftranslation, Frotation, Fscale, *transform.m);

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

						tc.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(Fscale[0], Fscale[1], Fscale[2])
							* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(Frotation[0]), DirectX::XMConvertToRadians(Frotation[1]), DirectX::XMConvertToRadians(Frotation[2]))))
							* DirectX::XMMatrixTranslation(Ftranslation[0], Ftranslation[1], Ftranslation[2]);
					}
				}
				else
				{
					if(selectedEntity.HasComponent<MeshComponent>())
					{
						auto& mc = selectedEntity.GetComponent<MeshComponent>();

						DirectX::XMMATRIX transformBase = tc.Transform * mc.Mesh->GetLocalTransform();

						DirectX::XMMatrixDecompose(&scale, &rotation, &translation, transformBase);
						DirectX::XMStoreFloat3(&translationFloat3, translation);
						DirectX::XMStoreFloat3(&scaleFloat3, scale);
						DirectX::XMFLOAT4X4 transform;
						float Ftranslation[3] = { translationFloat3.x, translationFloat3.y, translationFloat3.z };
						float Frotation[3] = { tc.RotationEulerAngles.x, tc.RotationEulerAngles.y, tc.RotationEulerAngles.z };
						float Fscale[3] = { scaleFloat3.x, scaleFloat3.y, scaleFloat3.z };
						ImGuizmo::RecomposeMatrixFromComponents(Ftranslation, Frotation, Fscale, *transform.m);

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

							mc.Mesh->SetLocalTransform(DirectX::XMMatrixInverse(nullptr, tc.Transform) * DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(Fscale[0], Fscale[1], Fscale[2])
								* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(Frotation[0]), DirectX::XMConvertToRadians(Frotation[1]), DirectX::XMConvertToRadians(Frotation[2]))))
								* DirectX::XMMatrixTranslation(Ftranslation[0], Ftranslation[1], Ftranslation[2]));
						}
					}
				}
			}

			ImGui::End();
			ImGui::PopStyleVar();

			ImGui::End();

			ImGui::Begin(ICON_TOASTER_CALCULATOR" Statistics");

			std::string name = "none";
			if (mHoveredEntity)
				name = mHoveredEntity.GetComponent<TagComponent>().Tag;
			ImGui::Text("Hovered Entity: %s", name.c_str());

			ImGui::Text("FPS: %d", mEditorScene->GetFPS());
			ImGui::Text("Frame time: %fms", mEditorScene->GetFrameTime());
			ImGui::Text("Vertex count: %d", mEditorScene->GetVertices());

			ImGui::End();
		}
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
	}

	void EditorLayer::OnScenePlay()
	{
		mSceneState = SceneState::Play;

		mRuntimeScene = CreateRef<Scene>();
		mEditorScene->CopyTo(mRuntimeScene);

		mRuntimeScene->OnRuntimeStart();
		mSceneHierarchyPanel.SetContext(mRuntimeScene);
	}

	void EditorLayer::OnSceneStop()
	{
		mRuntimeScene->OnRuntimeStop();
		mSceneState = SceneState::Edit;
		mEditorScene->SetOldCameraTransform(DirectX::XMMatrixIdentity());

		mRuntimeScene = nullptr;

		ScriptEngine::SetSceneContext(mEditorScene);
		mSceneHierarchyPanel.SetContext(mEditorScene);
	}

	void EditorLayer::NewScene()
	{
		mEditorScene = CreateRef<Scene>();
		mEditorScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
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
		mSceneSettingsPanel.SetContext(mEditorScene);
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
			if (!ImGuizmo::IsUsing())
				mGizmoType = ImGuizmo::OPERATION::SCALE;

			break;
		}
		}

		return true;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{
		if (e.GetMouseButton() == Mouse::ButtonLeft)
		{
			if (mViewportHovered && !ImGuizmo::IsOver() && !Input::IsKeyPressed(Key::LeftAlt)) 
			{
				mSceneHierarchyPanel.SetSelectedEntity(mHoveredEntity);
				mEditorScene->SetSelectedEntity(mHoveredEntity);
			}
		}

		return true;
	}

	void EditorLayer::UpdateWindowTitle(const std::string& sceneName)
	{
		std::string title = sceneName + " - Toaster - " + Application::GetPlatformName() + " (" + Application::GetConfigurationName() + ")";
		Application::Get().GetWindow().SetTitle(title);
	}

}