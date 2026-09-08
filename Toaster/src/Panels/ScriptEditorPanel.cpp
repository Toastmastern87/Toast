#include "ScriptEditorPanel.h"

#include "Toast/Core/Log.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Utils/PlatformUtils.h"

#include <filesystem>

#include "../FontAwesome.h"

#include "imgui/imgui.h"

#include <fstream>;
#include <sstream>;

namespace Toast {

	ScriptEditorPanel::ScriptEditorPanel() 
	{
		mEditor.SetLanguage(TextEditor::Language::Cs());
	
		mEditor.SetPalette(TextEditor::GetDarkPalette());

		mEditor.SetShowWhitespacesEnabled(false);

		// Just a small test script to see if the colors are correct
		const char* sample =
			"// Toaster Script Editor\n"
			"public class Mover\n"
			"{\n"
			"    void OnUpdate(float ts)\n"
			"    {\n"
			"        // TODO: load a real .cs asset here in a later step\n"
			"    }\n"
			"}\n";
		mEditor.SetText(sample);
	}

	void ScriptEditorPanel::OnImGuiRender() 
	{
		if (!mOpen)
			return;

		std::string title = "Script Editor";
		if (!mCurrentFile.empty())
			title += " - " + mCurrentFile.filename().string();
		title += "###ScriptEditor";

		ImGui::Begin(title.c_str(), &mOpen);

		// Toolbar row
		// Save — disabled when nothing to save.
		ImGui::BeginDisabled(mCurrentFile.empty() || !IsDirty());
		if (ImGui::Button(ICON_TOASTER_SAVE""))
			Save();
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Save (Ctrl+S)");

		ImGui::SameLine();
		if (ImGui::Button(ICON_TOASTER_FOLDER_OPEN))
		{
			std::filesystem::path scriptsDir = mProjectPath / "Assets" / "Scripts";
			std::filesystem::create_directories(scriptsDir);

			std::optional<std::string> path = FileDialogs::OpenFile("C# Script\0*.cs\0", scriptsDir.string().c_str());

			if (path)
				OpenFile(*path);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Open script...");

		ImGui::SameLine();

		if (mFileType == EditorFileType::CSharp)
		{
			// Compile, always available if files has been changed outside of the editor
			if (ImGui::Button(ICON_TOASTER_COG))
				Compile();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Compile all scripts");
		}

		// Right-aligned filename + dirty marker
		if (!mCurrentFile.empty())
		{
			std::string label = mCurrentFile.filename().string();
			if (IsDirty())
				label += " *";

			float textW = ImGui::CalcTextSize(label.c_str()).x;
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - textW);
			ImGui::TextDisabled("%s", label.c_str());
		}

		ImGui::Separator();

		// Ctrl+S while the editor panel has focus.
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_S)))
			Save();

		mEditor.Render("##scriptEditor");

		ImGui::End();
	}

	void ScriptEditorPanel::SetProjectPath(const std::filesystem::path& projectPath, const std::string& projectName)
	{
		mProjectPath = projectPath;
		mProjectName = projectName;
	}

	void ScriptEditorPanel::OpenFile(const std::filesystem::path& filepath)
	{
		mCurrentFile = filepath;

		mFileType = (filepath.extension() == ".css") ? EditorFileType::StyleSheet : EditorFileType::CSharp;

		switch (mFileType)
		{
		case EditorFileType::CSharp:
			mEditor.SetLanguage(TextEditor::Language::Cs());
			break;
		case EditorFileType::StyleSheet:
			mEditor.SetLanguage(TextEditor::Language::C());
			break;
		}

		std::ifstream stream(filepath, std::ios::in | std::ios::binary);
		if (!stream)
		{
			TOAST_CORE_ERROR("ScriptEditorPanel: failed to open '%s'", filepath.string().c_str());
			return;
		}

		std::stringstream ss;
		ss << stream.rdbuf();

		mEditor.SetText(ss.str());
	}

	bool ScriptEditorPanel::Save()
	{
		if (mCurrentFile.empty())
			return false;

		std::ofstream out(mCurrentFile, std::ios::out | std::ios::binary);
		if (!out)
		{
			TOAST_CORE_ERROR("ScriptEditorPanel: Failed to write '%s'", mCurrentFile.string().c_str());
			return false;
		}

		out << mEditor.GetText();
		out.close();

		mSavedUndoIndex = mEditor.GetUndoIndex();

		return true;
	}

	bool ScriptEditorPanel::IsDirty()
	{
		// GetUndoIndex() advances with every edit and rewinds on undo, so equality
		// with the index at last save means the buffer matches the file — even if
		// the user edited and then undid everything.
		return !mCurrentFile.empty() && mEditor.GetUndoIndex() != mSavedUndoIndex;
	}

	void ScriptEditorPanel::Compile()
	{
		// Auto save first
		if (IsDirty())
		{
			if (!Save())
			{
				TOAST_CORE_ERROR("[Compile] Aborted: could not save '%s'", mCurrentFile.string().c_str());
				return;
			}
		}

		std::filesystem::path assetDir = mProjectPath / "Assets";
		std::filesystem::path outputDll = mProjectPath / "Binaries" / (ScriptEngine::SanitizeNamespace(mProjectName) + ".dll");

		ScriptEngine::CompileScripts(assetDir, outputDll);
	}

}