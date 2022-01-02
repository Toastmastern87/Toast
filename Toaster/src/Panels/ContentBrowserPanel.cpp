#include "tpch.h"

#include "ContentBrowserPanel.h"

#include "Toast/Renderer/Shader.h"

#include "../FontAwesome.h"

#include "imgui/imgui.h"

namespace Toast {

	// Once Toast Engine have "projects", change this
	extern const std::filesystem::path gAssetPath = "assets";

	ContentBrowserPanel::ContentBrowserPanel()
		: mCurrentDirectory(gAssetPath)
	{
		mDirectoryIcon = TextureLibrary::LoadTexture2D("Resources/Icons/ContentBrowser/DirectoryIcon.png");
		mFileIcon = TextureLibrary::LoadTexture2D("Resources/Icons/ContentBrowser/FileIcon.png");
	}

	void Toast::ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_FOLDER" Content Browser");

		if (mCurrentDirectory != std::filesystem::path(gAssetPath))
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
			auto relativePath = std::filesystem::relative(path, gAssetPath);
			std::string filenameStr = relativePath.filename().string();

			ImGui::PushID(filenameStr.c_str());
			Texture2D* icon = directoryEntry.is_directory() ? mDirectoryIcon : mFileIcon;
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::ImageButton(icon->GetID(), { thumbnailSize, thumbnailSize }, { 0, 0 }, { 1, 1 });

			// Check if file is a shader file
			if (filenameStr.find(".hlsl") != std::string::npos)
			{
				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::Button("Reload shader"))
					{
						ShaderLibrary::Reload(path.string().c_str());
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
			}

			if (ImGui::BeginDragDropSource())
			{
				const wchar_t* itemPath = relativePath.c_str();
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t), ImGuiCond_Once);
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (directoryEntry.is_directory())
					mCurrentDirectory /= path.filename();
			}
			ImGui::TextWrapped(filenameStr.c_str());

			ImGui::NextColumn();

			ImGui::PopID();
		}

		ImGui::End();
	}

}