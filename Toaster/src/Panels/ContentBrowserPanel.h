#pragma once

#include "Toast/Renderer/Texture.h"

#include <filesystem>

namespace Toast {

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel() = default;
		~ContentBrowserPanel() = default;

		void SetProjectPath(const std::filesystem::path& projectPath);
		void OnImGuiRender();

		void SetOpenFileCallback(std::function<void(const std::filesystem::path&)> callback)
		{
			mOpenFileCallback = callback;
		}
	private:
		std::filesystem::path mAssetRoot;
		std::filesystem::path mCurrentDirectory;

		Texture2D* mDirectoryIcon;
		Texture2D* mFileIcon;
		Texture2D* mFileIconCSharp;
		bool mInitialized = false;

		std::function<void(const std::filesystem::path&)> mOpenFileCallback;
	};

}