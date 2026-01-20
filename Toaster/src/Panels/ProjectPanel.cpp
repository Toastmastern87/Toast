#include "ProjectPanel.h"

#include "../FontAwesome.h"
#include "imgui/imgui.h"

namespace Toast {

	static float CalcListBoxHeight(float visibleItems)
	{
		const float rowH = ImGui::GetFrameHeight();
		const float rowGap = 1.0f; // matches your Dummy spacing
		const float innerPadY = 8.0f * 2.0f; // similar to your box padding estimate
		return innerPadY + visibleItems * rowH + (visibleItems - 1.0f) * rowGap;
	}

	void ProjectPanel::OnImGuiRender()
	{
		if (!mContext)
			return;

		ImGui::Begin(ICON_TOASTER_FOLDER_OPEN " Project");

		// ---------- Project Header ----------
		{
			ImGui::TextUnformatted("Project");
			ImGui::SameLine();
			ImGui::TextDisabled("%s", mContext->GetName().c_str());

			ImGui::TextDisabled("%s", mContext->GetPath().string().c_str());

			ImGui::Separator();
			ImGui::Spacing();
		}

		// ---------- Scenes Box (header + list inside the child) ----------
		{
			const float fullW = ImGui::GetContentRegionAvail().x;
			const float boxH = CalcListBoxHeight(6.0f);

			ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoMove;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
			ImGui::BeginChild("##ProjectScenesBox", ImVec2(fullW, boxH), true, childFlags);

			// --- Header row INSIDE the box ---
			{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Scenes");

				const float btnSize = ImGui::GetFrameHeight(); // square
				float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnSize;

				ImGui::SameLine();
				ImGui::SetCursorPosX(rightX);

				ImGui::PushID("ProjectScenesAdd_InsideBox");
				if (ImGui::Button("+", ImVec2(btnSize, btnSize)))
					ImGui::OpenPopup("##ScenesAddPopup");

				if (ImGui::BeginPopup("##ScenesAddPopup"))
				{
					if (ImGui::MenuItem("New Scene..."))
					{
						// TODO (later)
					}
					if (ImGui::MenuItem("Import Scene..."))
					{
						// TODO (later)
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();

				ImGui::Separator();
			}

			// Small spacing below header
			ImGui::Dummy(ImVec2(0.0f, 2.0f));

			// --- List content ---
			const auto& scenes = mContext->GetScenes();
			UUID activeId = mContext->GetActiveSceneID();

			if (scenes.empty())
			{
				ImGui::TextDisabled("No scenes in this project yet.");
			}
			else
			{
				int i = 0;
				for (const auto& [id, entry] : scenes)
				{
					std::string name = entry.Path.stem().string(); // "Default Scene"

					ImGui::PushID(i++);

					const char* label = name.c_str();

					const bool isActive = (id == activeId);
					const bool isSelected = (mSelectedScene == id);
					const bool isRenamingThis = (mRenamingScene == id);

					// Boxed button styling (your established pattern)
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));

					// ----- Color logic -----
					// Priority: Active > Selected > Normal
					if (isActive)
					{
						// Active scene tint (subtle but unmistakable)
						ImVec4 base = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];

						// Slightly lift brightness for "loaded" feeling
						base.x *= 1.20f;
						base.y *= 1.20f;
						base.z *= 1.20f;

						ImGui::PushStyleColor(ImGuiCol_Button, base);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
					}
					else if (isSelected)
					{
						ImGui::PushStyleColor(
							ImGuiCol_Button,
							ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]
						);
					}

					float w = ImGui::GetContentRegionAvail().x;
					bool clicked = false;

					if (isRenamingThis)
					{
						ImGui::PushItemWidth(w);

						if (mRenameWantsFocus)
						{
							ImGui::SetKeyboardFocusHere();
							mRenameWantsFocus = false;
						}

						bool commit = ImGui::InputText("##RenameSceneInline", mRenameBuffer, sizeof(mRenameBuffer),	ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

						// Cancel on Escape
						if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
							mRenamingScene = UUID{};
						else if (commit)
						{
							std::string newName(mRenameBuffer);

							// Basic trim
							while (!newName.empty() && newName.front() == ' ')
								newName.erase(newName.begin());
							while (!newName.empty() && newName.back() == ' ')
								newName.pop_back();

							if (!newName.empty())
								mContext->RenameScene(id, newName);

							mRenamingScene = UUID{};
						}
						else
						{
							// If user clicks elsewhere, cancel rename
							if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
								mRenamingScene = UUID{};
						}

						ImGui::PopItemWidth();
					}
					else
					{
						clicked = ImGui::Button(label, ImVec2(w, 0.0f));
						if (clicked)
							mSelectedScene = id;
					}

					if (!isRenamingThis && ImGui::BeginPopupContextItem("##SceneContext", ImGuiPopupFlags_MouseButtonRight))
					{
						if (ImGui::MenuItem("Rename"))
						{
							mRenamingScene = id;
							mRenameWantsFocus = true;
							memset(mRenameBuffer, 0, sizeof(mRenameBuffer));

							std::string currentName = entry.Path.stem().string();
							strncpy(mRenameBuffer, currentName.c_str(), sizeof(mRenameBuffer) - 1);

							ImGui::CloseCurrentPopup();
						}
						if (ImGui::MenuItem("Remove from Project"))
						{
							// TODO (later)
						}

						ImGui::EndPopup();
					}

					ImGui::Dummy(ImVec2(0.0f, 0.5f));

					if (isActive)
						ImGui::PopStyleColor(3);
					else if (isSelected)
						ImGui::PopStyleColor(1);

					ImGui::PopStyleVar(3);

					ImGui::PopID();
				}
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();
		}

		ImGui::End();
	}


}
