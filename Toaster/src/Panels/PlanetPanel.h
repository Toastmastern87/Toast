#pragma once

#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Renderer/PlanetSystem.h"

#include "Toast/Scene/Scene.h"

namespace Toast {

	class PlanetPanel
	{
	public:
		PlanetPanel() = default;
		~PlanetPanel() = default;

		void OnImGuiRender(bool* showPanel, std::string& activeDragArea);

		void SetContext(Scene* sceneContext, WindowsWindow* window);
		void SetProjectPath(const std::filesystem::path& projectPath);
	private:
		void DrawTerrainObjectsListUI();
		void DrawTerrainObjectPopup();
		void DrawTerrainObjectMeshRow(TerrainObject& target);
	private:
		Scene* mSceneContext = nullptr;
		Planet* mContext = nullptr;

		std::filesystem::path mAssetRoot;

		WindowsWindow* mWindow;

		// Terrain Height Details UI state 
		bool mDetailPopupOpen = false;
		bool mEditingDetail = false;
		int mEditingDetailIndex = -1;
		bool mRequestOpenTerrainDetailPopup = false;
		HeightDetail mDetailDraft{};
		char mDetailNameBuf[128] = "New Terrain Detail";

		// Terrain Object UI state
		bool mEditingTerrainObj = false;
		int  mEditingTerrainObjIndex = -1;
		TerrainObject mTerrainObjDraft{};
		bool mRequestOpenTerrainObjPopup = false;
		char mTerrainObjNameBuf[128]{};
		char mTerrainObjMeshPathBuf[256]{};

		// Import popup
		std::filesystem::path mPendingImportPath;
		bool mPendingImportSRGB = true;
		bool mPendingImportOpen = false;

		// Callback to run after import completes — different for each texture slot
		std::function<void(AssetHandle)> mOnImportComplete;

		void DrawImportTexturePopup();
		void RequestTextureImport(const std::filesystem::path& path, bool defaultSRGB, std::function<void(AssetHandle)> onComplete);
	};

}