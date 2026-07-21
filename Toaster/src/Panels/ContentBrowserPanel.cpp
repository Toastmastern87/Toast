#include "tpch.h"

#include "ContentBrowserPanel.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Renderer/Shader.h"

#include "../FontAwesome.h"

#include "imgui/imgui.h"

namespace Toast {

	void ContentBrowserPanel::SetProjectPath(const std::filesystem::path& projectPath)
	{
		mAssetRoot = projectPath / "Assets";
		mCurrentDirectory = mAssetRoot;

		if (!mInitialized)
		{
			mDirectoryIcon = TextureLibrary::LoadTexture2D(
				"Resources/Icons/ContentBrowser/DirectoryIcon.png");
			mFileIcon = TextureLibrary::LoadTexture2D(
				"Resources/Icons/ContentBrowser/FileIcon.png");
			mFileIconCSharp = TextureLibrary::LoadTexture2D(
				"Resources/Icons/ContentBrowser/FileIconCSharp.png");
			mInitialized = true;
		}
	}

	void Toast::ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_FOLDER" Content Browser");

		if (!mInitialized)
		{
			ImGui::Text("No project loaded");
			ImGui::End();
			return;
		}

		// Only show back button if we're deeper than the asset root
		if (mCurrentDirectory != mAssetRoot)
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
			std::string filenameStr = path.filename().string();

			ImGui::PushID(filenameStr.c_str());

			Texture2D* icon;
			if (directoryEntry.is_directory())
				icon = mDirectoryIcon;
			else 
			{
				auto ext = path.extension().string();
				for (auto& c : ext) c = (char)std::tolower(c);

				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
					ext == ".bmp" || ext == ".tga" || ext == ".hdr" || ext == ".dds" || ext == ".tif")
				{
					auto relativePath = std::filesystem::relative(path, mAssetRoot);
					AssetHandle handle = AssetManager::ImportAsset(relativePath);
					auto tex = AssetManager::GetAsset<Texture2D>(handle);
					icon = tex ? tex.get() : mFileIcon;
				}
				else if (ext == ".cs")
					icon = mFileIconCSharp;
				else
					icon = mFileIcon;
			}

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::ImageButton("##thumbnailButton", icon->GetID(), {thumbnailSize, thumbnailSize}, {0, 0}, {1, 1});

			// Check if file is a shader file
			if (filenameStr.find(".hlsl") != std::string::npos)
			{
				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::Button("Reload shader"))
					{
						auto relativePath = std::filesystem::relative(path, mAssetRoot);
						AssetHandle handle = AssetManager::ImportAsset(relativePath);  // resolves to existing handle
						if (handle)
							AssetManager::ReloadAsset(handle);
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
			}

			if (ImGui::BeginDragDropSource())
			{
				auto relativePath = std::filesystem::relative(path, mAssetRoot);
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