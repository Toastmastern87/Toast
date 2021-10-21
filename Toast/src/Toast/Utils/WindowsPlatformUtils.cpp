#include "tpch.h"
#include "Toast/Utils/PlatformUtils.h"

#include "Toast/Core/Application.h"

#include <commdlg.h>
#include <iostream>
#include <filesystem>

namespace Toast {

	std::optional<std::string> FileDialogs::OpenFile(const char* filter, const char* dir)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };

		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = (HWND)Application::Get().GetWindow().GetNativeWindow();
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.lpstrInitialDir = dir;
		ofn.lpstrDefExt = strchr(filter, '\0') + 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return std::nullopt;
	}

	std::optional<std::string> FileDialogs::SaveFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };

		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = (HWND)Application::Get().GetWindow().GetNativeWindow();
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetSaveFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return std::nullopt;
	}

	bool FileDialogs::DeleteFile(const std::string& path)
	{
		std::filesystem::remove(path);

		return true;
	}

	std::vector<std::string> FileDialogs::GetAllFiles(std::string path)
	{
		std::vector<std::string> fileStrings;

		std::string directPath = std::filesystem::current_path().string() + path;

		for (const auto& entry : std::filesystem::directory_iterator(directPath))
			fileStrings.push_back(entry.path().string());

		return fileStrings;
	}
}