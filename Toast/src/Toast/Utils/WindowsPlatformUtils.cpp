#include "tpch.h"
#include "Toast/Utils/PlatformUtils.h"

#include "Toast/Core/Application.h"

#include <shobjidl.h> 
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

	std::optional<std::string> FileDialogs::OpenFolder(const char* initialDir)
	{
		// Initialize COM for this thread.
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		if (FAILED(hr))
			return std::nullopt;

		IFileDialog* fileDialog = nullptr;
		hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fileDialog));
		if (FAILED(hr))
		{
			CoUninitialize();
			return std::nullopt;
		}

		// Set the dialog to select folders.
		DWORD dwOptions;
		if (SUCCEEDED(fileDialog->GetOptions(&dwOptions)))
		{
			hr = fileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
		}

		// If an initial folder was provided, set it.
		if (initialDir && SUCCEEDED(hr))
		{
			int size_needed = MultiByteToWideChar(CP_UTF8, 0, initialDir, -1, nullptr, 0);
			if (size_needed > 0)
			{
				std::wstring wInitialDir(size_needed, 0);
				MultiByteToWideChar(CP_UTF8, 0, initialDir, -1, &wInitialDir[0], size_needed);

				IShellItem* pItem = nullptr;
				hr = SHCreateItemFromParsingName(wInitialDir.c_str(), nullptr, IID_PPV_ARGS(&pItem));
				if (SUCCEEDED(hr))
				{
					fileDialog->SetFolder(pItem);
					pItem->Release();
				}
			}
		}

		std::optional<std::string> result = std::nullopt;
		// Show the dialog using your application's native window handle.
		if (SUCCEEDED(fileDialog->Show((HWND)Application::Get().GetWindow().GetNativeWindow())))
		{
			IShellItem* resultItem = nullptr;
			hr = fileDialog->GetResult(&resultItem);
			if (SUCCEEDED(hr))
			{
				PWSTR pszPath = nullptr;
				hr = resultItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
				if (SUCCEEDED(hr))
				{
					// Convert the wide string to a standard std::string.
					std::wstring ws(pszPath);
					result = std::string(ws.begin(), ws.end());
					CoTaskMemFree(pszPath);
				}
				resultItem->Release();
			}
		}

		fileDialog->Release();
		CoUninitialize();

		return result;
	}

}