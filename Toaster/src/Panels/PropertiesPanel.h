#pragma once

#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Core/Base.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Scene.h"

#include "SceneHierarchyPanel.h"

namespace Toast {

	class PropertiesPanel
	{
	public:
		PropertiesPanel() = default;
		PropertiesPanel(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel, WindowsWindow* window);
		~PropertiesPanel() = default;

		void SetContext(const Entity& context, SceneHierarchyPanel* sceneHierarchyPanel, WindowsWindow* window);
		void SetProjectPath(const std::filesystem::path& projectPath, const std::string& projectName);

		void OnImGuiRender(std::string& activeDragArea);

		void SetOpenScriptCallback(std::function<void(const std::filesystem::path&)> callback)
		{
			mOpenScriptCallback = callback;
		}
	private:
		void DrawComponents(Entity entity, std::string& activeDragArea);

		void DrawImportTexturePopup();
		void RequestTextureImport(const std::filesystem::path& path, bool defaultSRGB, std::function<void(AssetHandle)> onComplete);
		void RequestUITextureImport(const std::filesystem::path& path, bool defaultSRGB, std::function<void(AssetHandle)> onComplete);
	private:
		Entity mContext;
		Scene* mScene;
		std::filesystem::path mAssetRoot;
		std::string mProjectName;

		WindowsWindow* mWindow;

		SceneHierarchyPanel* mSceneHierarchyPanel;

		// Import popup
		std::filesystem::path mPendingImportPath;
		std::filesystem::path mPendingExportPath;
		bool mPendingImportSRGB = true;
		bool mPendingImportOpen = false;

		// Callback to run after import completes — different for each texture slot
		std::function<void(AssetHandle)> mOnImportComplete;

		std::function<void(const std::filesystem::path&)> mOpenScriptCallback;
	};

}