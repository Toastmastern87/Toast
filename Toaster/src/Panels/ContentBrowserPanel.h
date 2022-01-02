#pragma once

#include "Toast/Renderer/Texture.h"

#include <filesystem>

namespace Toast {

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel() = default;

		void OnImGuiRender();
	private:
		std::filesystem::path mCurrentDirectory;

		Texture2D* mDirectoryIcon;
		Texture2D* mFileIcon;
	};

}