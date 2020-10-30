#pragma once

#include "Toast.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneSettingsPanel.h"

namespace Toast {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& e) override;

		void OpenScene();
		void SaveScene();
		void SaveSceneAs();
		void NewScene();
	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		void UpdateWindowTitle(const std::string& sceneName);
	private:
		std::string mSceneFilePath = "";

		OrthographicCameraController mCameraController;

		Ref<Framebuffer> mFramebuffer;

		Ref<Scene> mActiveScene;

		Ref<PerspectiveCamera> mEditorCamera;

		Ref<Texture2D> mCheckerboardTexture;

		bool mViewportFocused = false, mViewportHovered = false;

		DirectX::XMFLOAT2 mViewportSize = { 0.0f, 0.0f };

		enum class SceneState
		{
			Edit = 0, Play = 1
		};
		SceneState mSceneState = SceneState::Edit;

		// Panels
		SceneHierarchyPanel mSceneHierarchyPanel;
		SceneSettingsPanel mSceneSettingsPanel;
	};
}