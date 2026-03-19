#include "ProjectPanel.h"

#include "Toast/Utils/PlatformUtils.h"

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

		{
			const float fullW = ImGui::GetContentRegionAvail().x;
			const float boxH = CalcListBoxHeight(6.0f);

			// --- Header row OUTSIDE the box ---
			{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Scenes");

				const float btnSize = ImGui::GetFrameHeight(); // square

				// Right-align the + button on the same line
				ImGui::SameLine();
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnSize);

				ImGui::PushID("ProjectScenesAdd_OutsideBox");
				if (ImGui::Button("+", ImVec2(btnSize, btnSize)))
					ImGui::OpenPopup("##ScenesAddPopup");

				if (ImGui::BeginPopup("##ScenesAddPopup"))
				{
					if (ImGui::MenuItem("New Scene..."))
					{
						UUID newId = mContext->CreateNewScene("NewScene", false);

						// Select it in the list
						mSelectedScene = newId;

						// Optional: start renaming immediately (recommended UX)
						mRenamingScene = newId;
						mRenameWantsFocus = true;
						memset(mRenameBuffer, 0, sizeof(mRenameBuffer));
						strncpy(mRenameBuffer, "NewScene", sizeof(mRenameBuffer) - 1);

						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("Import Scene..."))
					{
						std::optional<std::string> pathOpt =
							FileDialogs::OpenFile("Toast Scene (*.tscene)\0*.tscene\0", "..\\Toaster\\assets\\scenes\\");
						if (pathOpt)
						{
							std::filesystem::path srcAbs(*pathOpt);
							UUID imported = mContext->ImportScene(srcAbs, false);
							if (imported)
								mSelectedScene = imported;
						}

						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
				ImGui::PopID();
			}

			// A few pixels of padding between header and box
			ImGui::Dummy(ImVec2(0.0f, 2.0f));

			// --- Box (child) containing ONLY the list content ---
			ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoMove;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
			ImGui::BeginChild("##ProjectScenesBox", ImVec2(fullW, boxH), true, childFlags);

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
					const std::string name = entry.Path.stem().string();

					ImGui::PushID(i++);

					const bool isActive = (id == activeId);
					const bool isSelected = (mSelectedScene == id);
					const bool isRenamingThis = (mRenamingScene == id);

					// Boxed button styling
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));

					const float w = ImGui::GetContentRegionAvail().x;
					std::string itemLabel = isActive ? ("> " + name) : name;

					int pushedColors = 0;
					if (isActive)
					{
						ImVec4 base = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
						base.w = 0.95f;
						ImGui::PushStyleColor(ImGuiCol_Button, base);        ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, base); ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);  ++pushedColors;
					}
					else if (isSelected)
					{
						ImVec4 base = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
						base.w = 0.70f;
						ImGui::PushStyleColor(ImGuiCol_Button, base); ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]); ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);   ++pushedColors;
					}

					if (isRenamingThis)
					{
						ImGui::PushItemWidth(w);

						if (mRenameWantsFocus)
						{
							ImGui::SetKeyboardFocusHere();
							mRenameWantsFocus = false;
						}

						const bool commit =
							ImGui::InputText("##RenameSceneInline", mRenameBuffer, sizeof(mRenameBuffer),
								ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

						if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
						{
							mRenamingScene = UUID{};
						}
						else if (commit)
						{
							std::string newName(mRenameBuffer);
							while (!newName.empty() && newName.front() == ' ') newName.erase(newName.begin());
							while (!newName.empty() && newName.back() == ' ') newName.pop_back();

							if (!newName.empty())
								mContext->RenameScene(id, newName);

							mRenamingScene = UUID{};
						}
						else
						{
							if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
								mRenamingScene = UUID{};
						}

						ImGui::PopItemWidth();
					}
					else
					{
						if (isActive)
						{
							ImGui::BeginDisabled(true);
							ImGui::Button(itemLabel.c_str(), ImVec2(w, 0.0f));
							ImGui::EndDisabled();
						}
						else
						{
							if (ImGui::Button(itemLabel.c_str(), ImVec2(w, 0.0f)))
								mSelectedScene = id;

							if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							{
								mSelectedScene = id;
								if (OnOpenSceneRequested)
									OnOpenSceneRequested(id);
							}
						}
					}

					if (pushedColors > 0)
						ImGui::PopStyleColor(pushedColors);

					if (!isRenamingThis && ImGui::BeginPopupContextItem("##SceneContext", ImGuiPopupFlags_MouseButtonRight))
					{
						if (ImGui::MenuItem("Rename"))
						{
							mRenamingScene = id;
							mRenameWantsFocus = true;
							memset(mRenameBuffer, 0, sizeof(mRenameBuffer));
							const std::string currentName = entry.Path.stem().string();
							strncpy(mRenameBuffer, currentName.c_str(), sizeof(mRenameBuffer) - 1);
							ImGui::CloseCurrentPopup();
						}

						if (isActive)
						{
							ImGui::BeginDisabled(true);
							ImGui::MenuItem("Delete (active scene)");
							ImGui::EndDisabled();

							ImGui::Separator();
							ImGui::TextDisabled("Set another scene active before deleting.");
						}
						else
						{
							if (ImGui::MenuItem("Delete"))
							{
								mPendingDeleteScene = id;
								mDeletePopupOpen = true;

								memset(mDeleteSceneName, 0, sizeof(mDeleteSceneName));
								const std::string currentName = entry.Path.stem().string();
								strncpy(mDeleteSceneName, currentName.c_str(), sizeof(mDeleteSceneName) - 1);

								ImGui::CloseCurrentPopup();
							}
						}

						ImGui::EndPopup();
					}

					ImGui::PopStyleVar(3);
					ImGui::PopID();
				}
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();
		}

		if (mDeletePopupOpen)
		{
			ImGui::OpenPopup("##ConfirmDeleteScene");
			mDeletePopupOpen = false;
		}

		bool confirmDelete = false;

		if (ImGui::BeginPopupModal("##ConfirmDeleteScene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Delete scene?");
			ImGui::Spacing();

			ImGui::Text("Scene: %s", mDeleteSceneName[0] ? mDeleteSceneName : "<unknown>");
			ImGui::Spacing();

			ImGui::TextDisabled("This will permanently delete the scene and its .tscene file.");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Buttons
			const float btnW = 120.0f;

			// "Delete" (danger)
			if (ImGui::Button("Delete", ImVec2(btnW, 0.0f)))
			{
				confirmDelete = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			// "Cancel"
			if (ImGui::Button("Cancel", ImVec2(btnW, 0.0f)))
			{
				mPendingDeleteScene = UUID{};
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (confirmDelete && mPendingDeleteScene)
		{
			const UUID toDelete = mPendingDeleteScene;
			mPendingDeleteScene = UUID{};

			// Clear panel state if it points to the deleted scene
			if (mSelectedScene == toDelete)
				mSelectedScene = UUID{};
			if (mRenamingScene == toDelete)
				mRenamingScene = UUID{};

			// Perform deletion (includes deleting the serialized file)
			mContext->DeleteScene(toDelete);
		}

		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button(ICON_TOASTER_ROCKET " Build Game", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
		{
			mContext->BuildGame();
		}

		ImGui::Spacing();

		ImGui::End();
	}

}
