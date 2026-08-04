#pragma once

#include "../vendor/ImGuiColorTextEdit/TextEditor.h"

#include <filesystem>

namespace Toast {

	class ScriptEditorPanel
	{
	public:
		ScriptEditorPanel();

		void OnImGuiRender();

		void SetProjectPath(const std::filesystem::path& projectPath);

		void OpenFile(const std::filesystem::path& filepath);
		bool Save();
		bool IsDirty();

		bool IsOpen() const { return mOpen; }
		void SetOpen(bool open) { mOpen = open; }
	private:
		void Compile();
	private:
		TextEditor mEditor;
		std::filesystem::path mCurrentFile;
		int mSavedUndoIndex = 0;
		bool mOpen = false;

		std::filesystem::path mProjectPath;
		std::string mProjectName;
	};

}