#pragma once

#include "Toast/Core/Layer.h"
#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Events/KeyEvent.h"

#include "Toast/Renderer/Texture.h"

#include "Toast/Utils/PlatformUtils.h"

#include "Toast/Project/Project.h"

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneSettingsPanel.h"
#include "Panels/MaterialPanel.h"
#include "Panels/EnvironmentalPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/PropertiesPanel.h"

namespace Toast {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer(WindowsWindow* window = nullptr);
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& e) override;

		void OnScenePlay();
		void OnScenePause();
		void OnSceneUnpause();
		void OnSceneStop();

		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void SaveScene();
		void SaveSceneAs();
		void NewScene();
	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);
		bool OnMouseMoved(MouseMovedEvent& e);
		void UpdateWindowTitle(const std::string& sceneName);
		void UpdateWindowIcon(const std::string& iconPath);

		void RenderCustomTitleBar();

		void ShowCreateNewProject();
	private:
		std::optional<std::string> mSceneFilePath;

		Ref<Project> mProject;

		Ref<Scene> mRuntimeScene, mEditorScene;

		Ref<EditorCamera> mEditorCamera;

		Texture2D* mCheckerboardTexture;
		Texture2D* mPlayButtonTex;
		Texture2D* mPauseButtonTex;
		Texture2D* mStopButtonTex;
		Texture2D* mLogoTex;
		Texture2D* mCloseButtonTex;
		Texture2D* mMinButtonTex;
		Texture2D* mMaxButtonTex;

		Entity mHoveredEntity;

		bool mViewportFocused = false, mViewportHovered = false;

		WindowsWindow* mWindow;
		
		std::string mActiveDragArea;

		DirectX::XMFLOAT2 mViewportSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 mPreviousViewportSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 mViewportBounds[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
		DirectX::XMFLOAT2 mAbsoluteViewportPos = { 0.0f, 0.0f };

		enum class SceneState
		{
			Edit = 0, Play = 1, Pause = 2
		};
		SceneState mSceneState = SceneState::Edit;

		int mGizmoType = -1;

		bool mShowNewProjectPopup = false;

		// Panels
		SceneHierarchyPanel mSceneHierarchyPanel;
		SceneSettingsPanel mSceneSettingsPanel;
		MaterialPanel mMaterialPanel;
		EnvironmentalPanel mEnvironmentPanel;
		ConsolePanel mConsolePanel;
		ContentBrowserPanel mContentBrowserPanel;
		PropertiesPanel mPropertiesPanel;
	};
}