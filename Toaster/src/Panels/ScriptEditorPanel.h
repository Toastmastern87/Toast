#pragma once

#include "../vendor/ImGuiColorTextEdit/TextEditor.h"

#include <filesystem>

namespace Toast {

	enum class EditorFileType : uint8_t 
	{
		CSharp = 0,
		StyleSheet = 1,
	};

	class ScriptEditorPanel
	{
	public:
		ScriptEditorPanel();

		void OnImGuiRender();

		void SetProjectPath(const std::filesystem::path& projectPath, const std::string& projectName);

		void OpenFile(const std::filesystem::path& filepath);
		bool Save();
		bool IsDirty();

		bool IsOpen() const { return mOpen; }
		void SetOpen(bool open) { mOpen = open; }

		void Compile();
	private:
		TextEditor mEditor;
		std::filesystem::path mCurrentFile;
		EditorFileType mFileType = EditorFileType::CSharp;

		int mSavedUndoIndex = 0;
		bool mOpen = false;

		std::filesystem::path mProjectPath;
		std::string mProjectName;
	};

}