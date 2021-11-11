#include "tpch.h"

#include "ContentBrowserPanel.h"

#include "../FontAwesome.h"

#include "imgui/imgui.h"

namespace Toast {

	// Once Toast Engine have "projects", change this
	static const std::filesystem::path sAssetPath = "assets";

	ContentBrowserPanel::ContentBrowserPanel()
		: mCurrentDirectory(sAssetPath)
	{
	}

	void Toast::ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_FOLDER" Content Browser");

		if (mCurrentDirectory != std::filesystem::path(sAssetPath))
		{
			if (ImGui::Button("<-"))
			{
				mCurrentDirectory = mCurrentDirectory.parent_path();
			}
		}

		for(auto& directoryEntry : std::filesystem::directory_iterator(mCurrentDirectory))
		{
			const auto& path = directoryEntry.path();
			auto relativePath = std::filesystem::relative(path, sAssetPath);
			std::string filenameStr = relativePath.filename().string();
			if (directoryEntry.is_directory())
			{
				if (ImGui::Button(filenameStr.c_str()))
				{
					mCurrentDirectory /= path.filename();
				}
			}
			else
			{
				if (ImGui::Button(filenameStr.c_str()))
				{
				}
			}
		}

		ImGui::End();
	}

}