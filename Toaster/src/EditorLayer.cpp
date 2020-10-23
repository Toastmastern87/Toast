#include "EditorLayer.h"

#include <imgui/imgui.h>
#include <filesystem>

#include "Toast/Renderer/Mesh.h"

namespace Toast {

	EditorLayer::EditorLayer()
		: Layer("TheNextFrontier2D"), mCameraController(1280.0f / 720.0f, true)
	{
	}

	void EditorLayer::OnAttach()
	{
		TOAST_PROFILE_FUNCTION();

		mCheckerboardTexture = Texture2D::Create("assets/textures/Checkerboard.png", 1);

		FramebufferSpecification fbSpec;
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		fbSpec.BuffersDesc.emplace_back(FramebufferSpecification::BufferDesc(TOAST_FORMAT_R32G32B32A32_FLOAT, TOAST_BIND_RENDER_TARGET | TOAST_BIND_SHADER_RESOURCE));
		fbSpec.BuffersDesc.emplace_back(FramebufferSpecification::BufferDesc(TOAST_FORMAT_D24_UNORM_S8_UINT, TOAST_BIND_DEPTH_STENCIL));
		mFramebuffer = Framebuffer::Create(fbSpec);
		
		mActiveScene = CreateRef<Scene>();

		mCameraEntity = mActiveScene->CreateEntity("Perspective Camera");
		mCameraEntity.AddComponent<CameraComponent>();

		class CameraController : public ScriptableEntity
		{
		public:
			void OnCreate() 
			{
				auto& translation = GetComponent<TransformComponent>().Translation;
				translation = { 0.0f, 3.0f, -12.0f };
			}

			void OnDestroy() 
			{

			}

			void OnUpdate(Timestep ts) 
			{
				auto& translation = GetComponent<TransformComponent>().Translation;
				float speed = 5.0f;

				if (Input::IsKeyPressed(Key::A))
					translation.x -= speed * ts;
				if (Input::IsKeyPressed(Key::D))
					translation.x += speed * ts;
				if (Input::IsKeyPressed(Key::E))
					translation.y += speed * ts;
				if (Input::IsKeyPressed(Key::Q))
					translation.y -= speed * ts;
				if (Input::IsKeyPressed(Key::W))
					translation.z += speed * ts;
				if (Input::IsKeyPressed(Key::S))
					translation.z -= speed * ts;
			}
		};

		mCameraEntity.AddComponent<NativeScriptComponent>().Bind<CameraController>();

		mSceneHierarchyPanel.SetContext(mActiveScene);
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
			mCameraController.OnResize(mViewportSize.x, mViewportSize.y);

			mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
		}

		// Update
		if (mViewportFocused)
			mCameraController.OnUpdate(ts);

		// Render
		Renderer2D::ResetStats();
		mFramebuffer->Bind();
		mFramebuffer->Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

		// Update scene
		mActiveScene->OnUpdate(ts);

		RenderCommand::BindBackbuffer();
		RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
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
					if (ImGui::MenuItem("New Scene", "Ctrl-N"))
						;//TODO
					if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
						OpenScene();
					ImGui::Separator();
					if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
						SaveScene();
					if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
						SaveSceneAs();

					ImGui::Separator();
					if (ImGui::MenuItem("Exit")) 
						Toast::Application::Get().Close();

					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}

			mSceneHierarchyPanel.OnImGuiRender();

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
	}

	void EditorLayer::OpenScene()
	{
		auto& app = Application::Get();
		std::string filepath = app.OpenFile("Toast Scene (*.tsc)\0*.tsc\0");

		if (!filepath.empty())
		{
			Ref<Scene> newScene = CreateRef<Scene>();
			SceneSerializer serializer(newScene);
			serializer.DeserializeScene(filepath);
			mActiveScene = newScene;
			std::filesystem::path path = filepath;
			UpdateWindowTitle(path.filename().string());
			mSceneHierarchyPanel.SetContext(mActiveScene);

			mSceneFilePath = filepath;
		}
	}

	void EditorLayer::SaveScene()
	{
		if (!(mSceneFilePath == "")) {
			SceneSerializer serializer(mActiveScene);
			serializer.SerializeScene(mSceneFilePath);
		}
		else {
			SaveSceneAs();
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		auto& app = Application::Get();
		std::string filepath = app.SaveFile("Toast Scene (*.tsc)\0*.tsc\0");

		if (!filepath.empty()) 
		{
			SceneSerializer serializer(mActiveScene);
			serializer.SerializeScene(filepath);

			std::filesystem::path path = filepath;
			UpdateWindowTitle(path.filename().string());
			mSceneFilePath = filepath;
		}
	}

	void EditorLayer::UpdateWindowTitle(const std::string& sceneName)
	{
		std::string title = sceneName + " - Toaster - " + Application::GetPlatformName() + " (" + Application::GetConfigurationName() + ")";
		Application::Get().GetWindow().SetTitle(title);
	}

}