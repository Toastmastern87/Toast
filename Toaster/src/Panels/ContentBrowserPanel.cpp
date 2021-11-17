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
		mDirectoryIcon = TextureLibrary::LoadTexture2D("Resources/Icons/ContentBrowser/DirectoryIcon.png");
		mFileIcon = TextureLibrary::LoadTexture2D("Resources/Icons/ContentBrowser/FileIcon.png");
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

		static float padding = 16.0f;
		static float thumbnailSize = 128.0f;
		float cellSize = thumbnailSize + padding;

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1)
			columnCount = 1;

		ImGui::Columns(columnCount, 0, false);

		for (auto& directoryEntry : std::filesystem::directory_iterator(mCurrentDirectory))
		{
			const auto& path = directoryEntry.path();
			auto relativePath = std::filesystem::relative(path, sAssetPath);
			std::string filenameStr = relativePath.filename().string();

			Ref<Texture2D> icon = directoryEntry.is_directory() ? mDirectoryIcon : mFileIcon;
			ImGui::ImageButton(icon->GetID(), { thumbnailSize, thumbnailSize }, { 0, 0 }, { 1, 1 });
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (directoryEntry.is_directory())
					mCurrentDirectory /= path.filename();
			}
			ImGui::TextWrapped(filenameStr.c_str());

			ImGui::NextColumn();
		}

		ImGui::End();
	}

}