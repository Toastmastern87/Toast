#pragma once

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
	};

}