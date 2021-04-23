#pragma once

#include "Toast/Core/Layer.h"
#include "Toast/Events/KeyEvent.h"

#include "Toast/Renderer/OrthographicCameraController.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/Texture.h"

#include "Toast/Utils/PlatformUtils.h"

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneSettingsPanel.h"
#include "Panels/MaterialPanel.h"
#include "Panels/EnvironmentalPanel.h"
#include "Panels/ConsolePanel.h"

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

		void OnScenePlay();
		void OnSceneStop();

		void OpenScene();
		void SaveScene();
		void SaveSceneAs();
		void NewScene();
	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		void UpdateWindowTitle(const std::string& sceneName);
	private:
		std::optional<std::string> mSceneFilePath;

		OrthographicCameraController mCameraController;

		Ref<Framebuffer> mFramebuffer;

		Ref<Scene> mRuntimeScene, mEditorScene;

		Ref<EditorCamera> mEditorCamera;

		Ref<Texture2D> mCheckerboardTexture;
		Ref<Texture2D> mPlayButtonTex;
		Ref<Texture2D> mPauseButtonTex;
		Ref<Texture2D> mStopButtonTex;

		Entity mHoveredEntity;

		bool mViewportFocused = false, mViewportHovered = false;

		DirectX::XMFLOAT2 mViewportSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 mViewportBounds[2];

		enum class SceneState
		{
			Edit = 0, Play = 1
		};
		SceneState mSceneState = SceneState::Edit;

		int mGizmoType = -1;

		// Panels
		SceneHierarchyPanel mSceneHierarchyPanel;
		SceneSettingsPanel mSceneSettingsPanel;
		MaterialPanel mMaterialPanel;
		EnvironmentalPanel mEnvironmentPanel;
		ConsolePanel mConsolePanel;
	};
}