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

namespace Toast {

	EditorLayer::EditorLayer()
		: Layer("TheNextFrontier2D"), mCameraController(1280.0f / 720.0f, true)
	{
	}

	void EditorLayer::OnAttach()
	{
		TOAST_PROFILE_FUNCTION();

		mCheckerboardTexture = CreateRef<Texture2D>("assets/textures/Checkerboard.png", 1, D3D11_PIXEL_SHADER);

		// Load all material shaders
		ShaderLibrary::Load("assets/shaders/Standard.hlsl");
		ShaderLibrary::Load("assets/shaders/PBR.hlsl");
		ShaderLibrary::Load("assets/shaders/Planet.hlsl");

		// Create standard material
		MaterialLibrary::Load("Standard", ShaderLibrary::Get("Standard"));

		// Load all materials from the computer
		std::vector<std::string> materialStrings = FileDialogs::GetAllFiles("\\assets\\materials");
		MaterialSerializer::Deserialize(materialStrings);

		FramebufferSpecification fbSpec;
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		fbSpec.BuffersDesc.emplace_back(FramebufferSpecification::BufferDesc(TOAST_FORMAT_R32G32B32A32_FLOAT, TOAST_BIND_RENDER_TARGET | TOAST_BIND_SHADER_RESOURCE));
		fbSpec.BuffersDesc.emplace_back(FramebufferSpecification::BufferDesc(TOAST_FORMAT_D24_UNORM_S8_UINT, TOAST_BIND_DEPTH_STENCIL));
		mFramebuffer = CreateRef<Framebuffer>(fbSpec);
		
		mActiveScene = CreateRef<Scene>();

		mEditorCamera = CreateRef<PerspectiveCamera>(45.0f);

		mSceneHierarchyPanel.SetContext(mActiveScene);
		mSceneSettingsPanel.SetContext(mActiveScene);
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

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
			ImGui::Begin("Viewport");

			mViewportFocused = ImGui::IsWindowFocused();
			mViewportHovered = ImGui::IsWindowHovered();
			
			Application::Get().GetImGuiLayer()->BlockEvents(!mViewportFocused || !mViewportHovered);

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
			mViewportSize = { viewportPanelSize.x, viewportPanelSize.y };		

			ImGui::Image(mFramebuffer->GetColorAttachmentID(), ImVec2{ mViewportSize.x, mViewportSize.y });
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

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(TOAST_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
	}

	void EditorLayer::NewScene()
	{
		mActiveScene = CreateRef<Scene>();
		mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		mSceneHierarchyPanel.SetContext(mActiveScene);
	}

	void EditorLayer::OpenScene()
	{
		std::string filepath = FileDialogs::OpenFile("Toast Scene(*.toast)\0*toast\0");
		if (!filepath.empty())
		{
			mActiveScene = CreateRef<Scene>();
			mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
			mSceneHierarchyPanel.SetContext(mActiveScene);
			mSceneSettingsPanel.SetContext(mActiveScene);

			std::filesystem::path path = filepath;
			UpdateWindowTitle(path.filename().string());
			mSceneFilePath = filepath;

			SceneSerializer serializer(mActiveScene);
			serializer.Deserialize(filepath);
		}
	}

	void EditorLayer::SaveScene()
	{
		if (!mSceneFilePath.empty()) {
			SceneSerializer serializer(mActiveScene);
			serializer.Serialize(mSceneFilePath);
		}
		else {
			SaveSceneAs();
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		mSceneFilePath = FileDialogs::SaveFile("Toast Scene(*.toast)\0*toast\0");
		if (!mSceneFilePath.empty())
		{
			SceneSerializer serializer(mActiveScene);
			serializer.Serialize(mSceneFilePath);

			std::filesystem::path path = mSceneFilePath;
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
		}

		return true;
	}

	void EditorLayer::UpdateWindowTitle(const std::string& sceneName)
	{
		std::string title = sceneName + " - Toaster - " + Application::GetPlatformName() + " (" + Application::GetConfigurationName() + ")";
		Application::Get().GetWindow().SetTitle(title);
	}

}