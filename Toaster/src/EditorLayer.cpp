#include "EditorLayer.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

#include "Toast/Core/Application.h"
#include "Toast/Core/Input.h"

#include "Toast/Scene/SceneSerializer.h"

#include <imgui/imgui.h>
#include <filesystem>

#include "Toast/Utils/PlatformUtils.h"

#include "ImGuizmo.h"

namespace Toast {

	EditorLayer::EditorLayer()
		: Layer("TheNextFrontier2D"), mCameraController(1280.0f / 720.0f, true)
	{
	}

	void EditorLayer::OnAttach()
	{
		TOAST_PROFILE_FUNCTION();

		mCheckerboardTexture = TextureLibrary::LoadTexture2D("assets/textures/Checkerboard.png");
		TextureLibrary::LoadTextureSampler("Default", D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);

		// Load all material shaders
		ShaderLibrary::Load("assets/shaders/Standard.hlsl");
		ShaderLibrary::Load("assets/shaders/PBR.hlsl");
		ShaderLibrary::Load("assets/shaders/Planet.hlsl");

		// Load all materials from the computer
		std::vector<std::string> materialStrings = FileDialogs::GetAllFiles("\\assets\\materials");
		MaterialSerializer::Deserialize(materialStrings);

		FramebufferSpecification fbSpec;
		fbSpec.Attachments = { FramebufferTextureFormat::R32G32B32A32_FLOAT, FramebufferTextureFormat::R32_SINT, FramebufferTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		mFramebuffer = CreateRef<Framebuffer>(fbSpec);
		
		mActiveScene = CreateRef<Scene>();

		mEditorCamera = CreateRef<EditorCamera>(30.0f, 1.778f, 0.01f, 30000.0f);

		mSceneHierarchyPanel.SetContext(mActiveScene);
		mSceneSettingsPanel.SetContext(mActiveScene);
		mEnvironmentPanel.SetContext(mActiveScene);
		mMaterialPanel.SetContext(MaterialLibrary::Get("Standard"));
	}

	void EditorLayer::OnDetach()
	{	
		TOAST_PROFILE_FUNCTION();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		TOAST_PROFILE_FUNCTION();

		// Resize
		if (FramebufferSpecification spec = mFramebuffer->GetSpecification();
			mViewportSize.x > 0.0f && mViewportSize.y > 0.0f && // zero sized framebuffer is invalid
			(spec.Width != mViewportSize.x || spec.Height != mViewportSize.y))
		{
			mFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

			switch (mSceneState)
			{
			case SceneState::Edit:
			{
				mEditorCamera->SetViewportSize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
				mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
				
				break;
			}
			case SceneState::Play:
			{
				mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

				break;
			}
			}
		}

		// Update
		if (mViewportFocused)
			mEditorCamera->OnUpdate(ts);

		// Render
		Renderer2D::ResetStats();
		mFramebuffer->Bind();
		mFramebuffer->Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

		// Update scene
		switch (mSceneState) 
		{
		case SceneState::Edit:
		{
			mActiveScene->OnUpdateEditor(ts, mEditorCamera);

			auto [mx, my] = ImGui::GetMousePos();
			mx -= mViewportBounds[0].x;
			my -= mViewportBounds[0].y;
			DirectX::XMFLOAT2 viewportSize = { mViewportBounds[1].x - mViewportBounds[0].x,  mViewportBounds[1].y - mViewportBounds[0].y };

			int mouseX = (int)mx;
			int mouseY = (int)my;

			if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y) 
			{
				int pixelData = mFramebuffer->ReadPixel(1, mouseX, mouseY);
				TOAST_CORE_WARN("Pixel data = {0}", pixelData);
			}

			break;
		}
		case SceneState::Play:
		{
			mActiveScene->OnUpdateRuntime(ts);

			break;
		}
		}

		RenderCommand::BindBackbuffer();
		RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

		// Make sure dirty materials are serialized
		MaterialLibrary::SerializeLibrary();
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

			mSceneSettingsPanel.OnImGuiRender();
			mSceneHierarchyPanel.OnImGuiRender();
			mMaterialPanel.OnImGuiRender();
			mEnvironmentPanel.OnImGuiRender();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
			ImGui::Begin("Viewport");
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

			ImGui::Image(mFramebuffer->GetColorAttachmentID(), ImVec2{ mViewportSize.x, mViewportSize.y });

			// Gizmos
			Entity selectedEntity = mSceneHierarchyPanel.GetSelectedEntity();
			if (selectedEntity && mGizmoType != -1)
			{
				ImGuizmo::SetOrthographic(false);
				ImGuizmo::SetDrawlist();
				float windowWidth = (float)ImGui::GetWindowWidth();
				float windowHeight = (float)ImGui::GetWindowHeight();
				ImGuizmo::SetRect(mViewportBounds[0].x, mViewportBounds[0].y, mViewportBounds[1].x - mViewportBounds[0].x, mViewportBounds[1].y - mViewportBounds[0].y);

				// Editor Camera
				DirectX::XMFLOAT4X4 cameraProjection;
				DirectX::XMStoreFloat4x4(&cameraProjection, mEditorCamera->GetProjection());
				DirectX::XMFLOAT4X4 cameraView;
				DirectX::XMStoreFloat4x4(&cameraView, mEditorCamera->GetViewMatrix());

				// Entity transform
				auto& tc = selectedEntity.GetComponent<TransformComponent>();
				DirectX::XMFLOAT4X4 transform;
				float Ftranslation[3] = { tc.Translation.x, tc.Translation.y, tc.Translation.z };
				float Frotation[3] = { DirectX::XMConvertToDegrees(tc.Rotation.x), DirectX::XMConvertToDegrees(tc.Rotation.y), DirectX::XMConvertToDegrees(tc.Rotation.z) };
				float Fscale[3] = { tc.Scale.x, tc.Scale.y, tc.Scale.z };
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

					tc.Translation = DirectX::XMFLOAT3(Ftranslation);
					tc.Rotation = DirectX::XMFLOAT3(DirectX::XMConvertToRadians(Frotation[0]), DirectX::XMConvertToRadians(Frotation[1]), DirectX::XMConvertToRadians(Frotation[2]));
					tc.Scale = DirectX::XMFLOAT3(Fscale);
				}
			}

			ImGui::End();
			ImGui::PopStyleVar();

			ImGui::End();
		}
		else
		{
			ImGui::Begin("Settings");

			auto stats = Renderer2D::GetStats();
			ImGui::Text("Renderer2D Stats: ");
			ImGui::Text("Draw Calls: %d", stats.DrawCalls);
			ImGui::Text("Quads: %d", stats.QuadCount);
			ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
			ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

			ImGui::Image(mCheckerboardTexture->GetID(), ImVec2{ 1280.0f, 720.0f });
			ImGui::End();
		}
	}

	void EditorLayer::OnEvent(Event& e)
	{
		mCameraController.OnEvent(e);
		mEditorCamera->OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
	}

	void EditorLayer::NewScene()
	{
		mActiveScene = CreateRef<Scene>();
		mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		mSceneHierarchyPanel.SetContext(mActiveScene);
		mEnvironmentPanel.SetContext(mActiveScene);
	}

	void EditorLayer::OpenScene()
	{
		std::optional<std::string> filepath = FileDialogs::OpenFile("Toast Scene(*.toast)\0*toast\0");
		if (filepath)
		{
			mActiveScene = CreateRef<Scene>();
			mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			mSceneHierarchyPanel.SetContext(mActiveScene);
			mSceneSettingsPanel.SetContext(mActiveScene);
			mEnvironmentPanel.SetContext(mActiveScene);

			std::filesystem::path path = *filepath;
			UpdateWindowTitle(path.filename().string());
			mSceneFilePath = *filepath;

			SceneSerializer serializer(mActiveScene);
			serializer.Deserialize(*filepath);
		}
	}

	void EditorLayer::SaveScene()
	{
		if (mSceneFilePath) {
			SceneSerializer serializer(mActiveScene);
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
			SceneSerializer serializer(mActiveScene);
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
				if(!ImGuizmo::IsUsing())
					mGizmoType = -1;

				break;
			}
			case Key::W:
			{
				if(!ImGuizmo::IsUsing())
					mGizmoType = ImGuizmo::OPERATION::TRANSLATE;

				break;
			}
			case Key::E:
			{
				if(!ImGuizmo::IsUsing())
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

	void EditorLayer::UpdateWindowTitle(const std::string& sceneName)
	{
		std::string title = sceneName + " - Toaster - " + Application::GetPlatformName() + " (" + Application::GetConfigurationName() + ")";
		Application::Get().GetWindow().SetTitle(title);
	}

}