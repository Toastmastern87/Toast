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

		Ref<Texture2D> mDirectoryIcon;
		Ref<Texture2D> mFileIcon;
	};

}