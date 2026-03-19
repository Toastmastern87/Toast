#pragma once

#include "Toast/Renderer/Material.h"

namespace Toast {

	class MaterialPanel
	{
	public:
		MaterialPanel() = default;
		~MaterialPanel() = default;

		void SetContext(const Ref<Material>& context);
		void SetProjectPath(const std::filesystem::path& projectPath);

		void OnImGuiRender();
	private:
		void DrawMaterialProperties();

		void DrawImportTexturePopup();
		void RequestTextureImport(const std::filesystem::path& path, bool defaultSRGB, std::function<void(AssetHandle)> onComplete);
	private:
		Ref<Material> mSelectionContext;
		std::filesystem::path mAssetRoot;

		// Import popup
		std::filesystem::path mPendingImportPath;
		bool mPendingImportSRGB = true;
		bool mPendingImportOpen = false;

		// Callback to run after import completes — different for each texture slot
		std::function<void(AssetHandle)> mOnImportComplete;
	};

}