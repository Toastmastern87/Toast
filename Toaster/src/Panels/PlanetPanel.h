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
	private:
		Scene* mSceneContext = nullptr;
		Planet* mContext = nullptr;

		WindowsWindow* mWindow;

		// --- Planet Details UI state ---
		bool mDetailPopupOpen = false;
		bool mEditingDetail = false;
		int mEditingDetailIndex = -1;
		bool mRequestOpenTerrainDetailPopup = false;

		// temp edit buffer
		HeightDetail mDetailDraft{};
		char mDetailNameBuf[128] = "New Terrain Detail";
	};

}