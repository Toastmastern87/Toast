#include "tpch.h"

#include "ImGuiHelpers.h"

#include "Toast/Renderer/UI/UIStyleSystem.h"

#include "Toast/Utils/PlatformUtils.h"

namespace Toast
{
	namespace ImGuiHelpers
	{

		struct DragState
		{
			float startValue;
			ImVec2 lastDelta;

			bool  isEditing = false;
			char  inputBuf[64] = {};
		};

		static std::unordered_map<ImGuiID, DragState> g_DragStates;

		bool ManualDragFloat(const char* label, float& value, WindowsWindow* window, std::string& activeDragArea, float speed, ImVec2 dragAreaSize, const char* displayFormat, float minVal, float maxVal)
		{
			ImGuiID id = ImGui::GetID(label);
			DragState& st = g_DragStates[id];

			bool changed = false;

			static bool justBecameEditing = false;

			// 2) We create an invisible button to capture clicks
			//    The size can be adjusted, or you can do a small arrow etc.
			ImGui::PushID(label);

			if (!st.isEditing)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0x2F, 0x31, 0x33, 0xFF)); // #2F3133
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0x4C, 0x4D, 0x4E, 0xFF)); 
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0x37, 0x39, 0x3B, 0xFF));

				ImGuiButtonFlags btnFlags = ImGuiButtonFlags_PressedOnClick;
				bool clicked = ImGui::ButtonEx("##DragArea", dragAreaSize, btnFlags);

				bool hovered = ImGui::IsItemHovered();
				ImGuiIO& io = ImGui::GetIO();

				if (hovered && io.MouseClickedCount[ImGuiMouseButton_Left] == 2)
				{
					justBecameEditing = true; 

					st.isEditing = true;
					// Copy current float to inputBuf
					snprintf(st.inputBuf, sizeof(st.inputBuf), displayFormat, value);
				}
				else if (clicked)
				{
					// Handle single click (e.g., start dragging)
					if (!window->IsDragging())
					{
						window->SetDragOnGoing(true);
						activeDragArea = label; // Set the active drag area
						st.startValue = value;
						st.lastDelta = ImVec2(0, 0);
					}
				}

				ImGui::PopStyleColor(3);

				// Now we overlay the numeric text in the center of that same button
				// We'll get the item rect to find out where the button is
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImVec2 rectMin = ImGui::GetItemRectMin(); // top-left of button
				ImVec2 rectMax = ImGui::GetItemRectMax(); // bottom-right of button
				ImVec2 center = ImVec2(
					(rectMin.x + rectMax.x) * 0.5f,
					(rectMin.y + rectMax.y) * 0.5f
				);

				// Format the numeric text
				char textBuf[64];
				snprintf(textBuf, sizeof(textBuf), displayFormat, value);

				// Compute text size so we can center it
				ImVec2 textSize = ImGui::CalcTextSize(textBuf);
				ImVec2 textPos = ImVec2(
					center.x - textSize.x * 0.5f,
					center.y - textSize.y * 0.5f
				);

				drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
					ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
					IM_COL32(255, 255, 255, 255), // White color
					textBuf);

				// 3) If we are in "dragging" mode, accumulate deltas into 'value'
				if (window->IsDragging() && activeDragArea == label)
				{
					bool leftDown = ImGui::GetIO().MouseDown[0];
					if (!leftDown)
					{
						window->SetDragOnGoing(false);
					}
					else
					{
						POINT delta = window->GetDeltaDrag(); // Get delta from WindowsWindow
						float dx = static_cast<float>(delta.x) - st.lastDelta.x;
						value += dx * speed;

						if (minVal != FLT_MAX && value < minVal)
							value = minVal;

						if (maxVal != 0.0f && value > maxVal)
							value = maxVal;

						changed = (dx != 0.0f);

						st.lastDelta.x = static_cast<float>(delta.x);
					}
				}
				else
				{
					// if user clicked => start dragging
					if (clicked)
					{
						if (!window->IsDragging())
						{
							window->SetDragOnGoing(true);
							st.startValue = value;
							st.lastDelta = ImVec2(0, 0);
						}
					}
				}
			}
			else
			{
				ImGui::SetNextItemWidth(dragAreaSize.x);

				if (justBecameEditing)
				{
					ImGui::SetKeyboardFocusHere();
					justBecameEditing = false; // Reset
				}

				ImVec2 startPos = ImGui::GetCursorScreenPos();

				if (ImGui::InputText("##FloatInput", st.inputBuf, IM_ARRAYSIZE(st.inputBuf),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
				{
					// user pressed Enter
					// parse
					float newVal = value;
					if (sscanf(st.inputBuf, "%f", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false; // done
				}

				// If user clicks away or otherwise defocuses, we finalize
				// We'll check ImGui::IsItemDeactivated() which is true
				// if user clicks outside or tab away
				if (ImGui::IsItemDeactivated() && st.isEditing)
				{
					float newVal = value;
					if (sscanf(st.inputBuf, "%f", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false;
				} 
			}

			ImGui::PopID();

			return changed;
		}

		bool ManualDragFloat2(const std::string& label, DirectX::XMFLOAT2& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea, const char* displayFormat /*= "%.1f"*/, bool colorValues /*= false*/, float overrideTotalWidth /*= 0.0f*/)
		{
			bool changed = false;

			ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;

			float availW = (overrideTotalWidth > 0.0f) ? overrideTotalWidth : ImGui::GetContentRegionAvail().x;

			size_t hashes = label.find("##");
			bool hasVisibleLabel = (hashes != 0);

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

			ImVec2 dragAreaSize;

			// layout example: we do columns or a simple horizontal layout
			ImGui::PushID(label.c_str());

			if (hasVisibleLabel)
			{
				dragAreaSize = { 54.0f, lineHeight };

				// layout example: we do columns or a simple horizontal layout
				ImGui::BeginTable("", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, availW * 0.30f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, availW * 0.65f);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped(label.c_str());

				ImGui::TableSetColumnIndex(1);
			}
			else
			{
				float totalButtons = 3.0f * buttonSize.x;

				float dragW = (availW - totalButtons) / 3.0f;
				if (dragW < 20.0f) dragW = 20.0f;

				dragAreaSize = { dragW, lineHeight };

				ImGui::BeginTable("", 1, ImGuiTableFlags_SizingStretchProp);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));

			char* labelCharX, *labelCharY;
			if (!colorValues)
			{
				labelCharX = "X";
				labelCharY = "Y";
			}
			else
			{
				labelCharX = "R";
				labelCharY = "G";
			}

			// We'll do X
			{
				// colored button for "X"
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushFont(boldFont);

				if (ImGui::Button(labelCharX, buttonSize))
				{
					values.x = resetValue;
					changed = true;
				}

				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea1 = "##" + label + "dragarea1";
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ManualDragFloat(dragArea1.c_str(), values.x, window, activeDragArea, speed, dragAreaSize, displayFormat);

				ImGui::SameLine();
			}

			// Y
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushFont(boldFont);

				if (ImGui::Button(labelCharY, buttonSize))
				{
					values.y = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea2 = "##" + label + "dragarea2";
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ManualDragFloat(dragArea2.c_str(), values.y, window, activeDragArea, speed, dragAreaSize, displayFormat);

				ImGui::SameLine();
			}

			ImGui::PopStyleVar();

			ImGui::EndTable();
			ImGui::PopID();

			return changed;
		}

		bool ManualDragDouble(const char* label, double& value, WindowsWindow* window, std::string& activeDragArea, float speed, ImVec2 dragAreaSize, const char* displayFormat, double minVal, double maxVal)
		{
			ImGuiID id = ImGui::GetID(label);
			DragState& st = g_DragStates[id];

			bool changed = false;

			// We create a push/pop ID to differentiate this widget in ImGui
			ImGui::PushID(label);

			// If not in editing mode (text input), we handle the drag area
			if (!st.isEditing)
			{
				// Style for the "button" background
				ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0x2F, 0x31, 0x33, 0xFF));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0x4C, 0x4D, 0x4E, 0xFF));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0x37, 0x39, 0x3B, 0xFF));

				// Use ButtonEx with 'PressedOnClick' so the click is recognized on mouse-down
				ImGuiButtonFlags btnFlags = ImGuiButtonFlags_PressedOnClick;
				bool clicked = ImGui::ButtonEx("##DragArea", dragAreaSize, btnFlags);

				bool hovered = ImGui::IsItemHovered();
				ImGuiIO& io = ImGui::GetIO();

				// Check double-click to switch to editing mode
				if (hovered && io.MouseClickedCount[ImGuiMouseButton_Left] == 2)
				{
					st.isEditing = true;

					// Copy current double into the input buffer
					// e.g. 2 decimal places
					std::snprintf(st.inputBuf, sizeof(st.inputBuf), displayFormat, value);
				}
				else if (clicked)
				{
					// Single click => start dragging if not already dragging
					if (!window->IsDragging())
					{
						window->SetDragOnGoing(true);
						activeDragArea = label;     // mark this as the active drag area
						st.startValue = value;
						st.lastDelta = ImVec2(0.0f, 0.0f);
					}
				}

				ImGui::PopStyleColor(3);

				// Overlay numeric text in the center of that "button" area
				// We'll get the item rect, compute center
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImVec2 rectMin = ImGui::GetItemRectMin();
				ImVec2 rectMax = ImGui::GetItemRectMax();
				ImVec2 center = ImVec2(
					(rectMin.x + rectMax.x) * 0.5f,
					(rectMin.y + rectMax.y) * 0.5f
				);

				// Format the numeric text (example: 1 decimal place)
				char textBuf[64];
				std::snprintf(textBuf, sizeof(textBuf), displayFormat, value);

				// Compute text size so we can center it
				ImVec2 textSize = ImGui::CalcTextSize(textBuf);

				// Draw text centered
				drawList->AddText(
					ImGui::GetFont(),
					ImGui::GetFontSize(),
					ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
					IM_COL32(255, 255, 255, 255),
					textBuf
				);

				// If dragging, accumulate deltas into 'value'
				if (window->IsDragging() && activeDragArea == label)
				{
					bool leftDown = io.MouseDown[0];
					if (!leftDown)
					{
						// Mouse released => stop dragging
						window->SetDragOnGoing(false);
						activeDragArea.clear();
					}
					else
					{
						// Use your window->GetDeltaDrag() or some method
						POINT delta = window->GetDeltaDrag();
						double dx = static_cast<double>(delta.x) - st.lastDelta.x;

						value += dx * speed;

						if (minVal != FLT_MAX && value < minVal)
							value = minVal;

						if (maxVal != 0.0f && value > maxVal)
							value = maxVal;

						changed = (dx != 0.0);

						st.lastDelta.x = static_cast<float>(delta.x);
					}
				}
				else
				{
					// If user clicked => start dragging (re-check in case above wasn't triggered)
					if (clicked)
					{
						if (!window->IsDragging())
						{
							window->SetDragOnGoing(true);
							st.startValue = value;
							st.lastDelta = ImVec2(0, 0);
						}
					}
				}
			}
			else
			{
				// -- Editing Mode (InputText) --

				ImGui::SetNextItemWidth(dragAreaSize.x);

				// Provide an InputText for the user to type the double
				if (ImGui::InputText(
					"##doubleInput",
					st.inputBuf,
					IM_ARRAYSIZE(st.inputBuf),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
				))
				{
					// user pressed Enter => parse the double
					double newVal = value;
					if (std::sscanf(st.inputBuf, "%lf", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false; // done editing
				}

				// If user clicks away or defocuses, finalize
				if (ImGui::IsItemDeactivated() && st.isEditing)
				{
					double newVal = value;
					if (std::sscanf(st.inputBuf, "%lf", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false;
				}
			}

			ImGui::PopID();
			return changed;
		}

		bool ManualDragFloat3(const std::string& label, DirectX::XMFLOAT3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea, const char* displayFormat, bool colorValues, float overrideTotalWidth)
		{
			bool changed = false;

			ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;

			float availW = (overrideTotalWidth > 0.0f) ? overrideTotalWidth : ImGui::GetContentRegionAvail().x;

			size_t hashes = label.find("##");
			bool hasVisibleLabel = (hashes != 0);

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

			ImVec2 dragAreaSize;

			// layout example: we do columns or a simple horizontal layout
			ImGui::PushID(label.c_str());

			if (hasVisibleLabel)
			{
				dragAreaSize = { 54.0f, lineHeight };

				// layout example: we do columns or a simple horizontal layout
				ImGui::BeginTable("", 2, flags);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, availW * 0.30f);
				ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, availW * 0.65f);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextWrapped(label.c_str());

				ImGui::TableSetColumnIndex(1);
			}
			else
			{
				float totalButtons = 3.0f * buttonSize.x;

				float dragW = (availW - totalButtons) / 3.0f;
				if (dragW < 20.0f) dragW = 20.0f;

				dragAreaSize = { dragW, lineHeight };

				ImGui::BeginTable("", 1, ImGuiTableFlags_SizingStretchProp);
				ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));

			char* labelCharX, * labelCharY, * labelCharZ;
			if (!colorValues)
			{
				labelCharX = "X";
				labelCharY = "Y";
				labelCharZ = "Z";
			}
			else
			{
				labelCharX = "R";
				labelCharY = "G";
				labelCharZ = "B";
			}

			// We'll do X
			{
				// colored button for "X"
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushFont(boldFont);

				if (ImGui::Button(labelCharX, buttonSize))
				{
					values.x = resetValue;
					changed = true;
				}

				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea1 = "##" + label + "dragarea1";
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ManualDragFloat(dragArea1.c_str(), values.x, window, activeDragArea, speed, dragAreaSize, displayFormat);

				ImGui::SameLine();
			}

			// Y
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushFont(boldFont);

				if (ImGui::Button(labelCharY, buttonSize))
				{
					values.y = resetValue;
					changed = true;
					}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea2 = "##" + label + "dragarea2";
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ManualDragFloat(dragArea2.c_str(), values.y, window, activeDragArea, speed, dragAreaSize, displayFormat);

				ImGui::SameLine();
			}

			// Z
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button(labelCharZ, buttonSize ))
				{
					values.z = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea3 = "##" + label + "dragarea3";
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ManualDragFloat(dragArea3.c_str(), values.z, window, activeDragArea, speed, dragAreaSize, displayFormat);
			}

			ImGui::PopStyleVar();

			ImGui::EndTable();
			ImGui::PopID();

			return changed;
		}

		bool ManualDragFloat3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea, const char* displayFormat, bool colorValues)
		{
			bool changed = false;

			ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			ImGui::PushID(label.c_str());

			ImGui::TableSetColumnIndex(0);
			ImGui::TextWrapped(label.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-1);

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
			ImVec2 dragAreaSize = { 54.0f, lineHeight };

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));

			char* labelCharX, * labelCharY, * labelCharZ;
			if (!colorValues)
			{
				labelCharX = "X";
				labelCharY = "Y";
				labelCharZ = "Z";
			}
			else
			{
				labelCharX = "R";
				labelCharY = "G";
				labelCharZ = "B";
			}

			// We'll do X
			{
				// colored button for "X"
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button(labelCharX, buttonSize))
				{
					values.x = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea1 = "##" + label + "dragarea1";

				changed |= ManualDragDouble(dragArea1.c_str(), values.x, window, activeDragArea, speed, dragAreaSize, displayFormat);

				ImGui::SameLine();
			}

			// Y
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button(labelCharY, buttonSize))
				{
					values.y = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea2 = "##" + label + "dragarea2";

				changed |= ManualDragDouble(dragArea2.c_str(), values.y, window, activeDragArea, speed, dragAreaSize, displayFormat);

				ImGui::SameLine();
			}

			// Z
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button(labelCharZ, buttonSize))
				{
					values.z = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea3 = "##" + label + "dragarea3";

				changed |= ManualDragDouble(dragArea3.c_str(), values.z, window, activeDragArea, speed, dragAreaSize, displayFormat);
			}

			ImGui::PopStyleVar();

			ImGui::PopItemWidth();

			ImGui::PopID();

			return changed;
		}

		bool ManualDragDouble3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea)
		{
			bool changed = false;

			ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			// layout example: we do columns or a simple horizontal layout
			ImGui::PushID(label.c_str());

			ImGui::TableSetColumnIndex(0);
			ImGui::TextWrapped(label.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-1);

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
			ImVec2 dragAreaSize = { 54.0f, lineHeight };

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));

			// We'll do X
			{
				// colored button for "X"
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("X", buttonSize))
				{
					values.x = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea1 = "##" + label + "dragarea1";

				changed |= ManualDragDouble(dragArea1.c_str(), values.x, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Y
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Y", buttonSize))
				{
					values.y = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea2 = "##" + label + "dragarea2";

				changed |= ManualDragDouble(dragArea2.c_str(), values.y, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Z
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Z", buttonSize))
				{
					values.z = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea3 = "##" + label + "dragarea3";

				changed |= ManualDragDouble(dragArea3.c_str(), values.z, window, activeDragArea, speed, dragAreaSize);
			}

			ImGui::PopStyleVar();

			ImGui::Columns(1);
			ImGui::PopID();

			return changed;
		}

		bool TinyExponentCombo(const char* id, int& exp10)
		{
			bool changed = false;

			// Smallest practical widths; tweak as needed
			ImGui::PushItemWidth(54.0f);

			const char* preview = nullptr;
			// Build preview like "e-10"
			char previewBuf[8];
			snprintf(previewBuf, sizeof(previewBuf), "e%d", exp10);
			preview = previewBuf;

			if (ImGui::BeginCombo(id, preview))
			{
				for (int e = -3; e >= -10; --e)
				{
					char buf[8];
					snprintf(buf, sizeof(buf), "e%d", e);

					bool isSelected = (exp10 == e);
					if (ImGui::Selectable(buf, isSelected))
					{
						exp10 = e;
						changed = true;
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::PopItemWidth();
			return changed;
		}

		bool ManualDragFloat3Scaled(const std::string& label, DirectX::XMFLOAT3& stored,	int exp10, float speedMantissa, float resetStored, WindowsWindow* window,	std::string& activeDragArea, const char* displayFormat,	bool colorValues, float overrideTotalWidth)
		{
			bool changed = false;

			const float scale = powf(10.0f, (float)exp10);

			// Mantissas shown to the user
			DirectX::XMFLOAT3 mantissa{ stored.x / scale, stored.y / scale,	stored.z / scale };

			// Use your existing widget, but operating on mantissa
			// IMPORTANT: resetValue here should reset mantissa, not stored.
			// If you want reset to zero mantissa, pass 0.0f.
			changed |= ManualDragFloat3(label, mantissa, speedMantissa,	0.0f,	window, activeDragArea, displayFormat, colorValues, overrideTotalWidth);

			if (changed)
			{
				stored.x = mantissa.x * scale;
				stored.y = mantissa.y * scale;
				stored.z = mantissa.z * scale;
			}

			return changed;
		}

		bool DragInt16(const char* label, int16_t* value, float speed, int min, int max)
		{
			int temp = *value;
			if (ImGui::DragInt(label, &temp, speed, min, max))
			{
				*value = static_cast<int16_t>(std::clamp(temp, min, max));
				return true;
			}
			return false;
		}

		std::string SanitiseFileName(const std::string& input)
		{
			std::string result;
			result.reserve(input.size());

			for (char c : input)
			{
				if (c == ' ')
				{
					result += '_';
					continue;
				}

				// Path separators and the Windows-reserved set. Letting any of these
				// through means create_directories writes somewhere unexpected.
				if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == '.')
					continue;

				if ((unsigned char)c < 32)
					continue;

				result += c;
			}

			// Trim leading and trailing underscores so " name " doesn't become "_name_".
			while (!result.empty() && result.front() == '_') result.erase(result.begin());
			while (!result.empty() && result.back() == '_')  result.pop_back();

			return result;
		}

		bool StyleOverrideMarker(UIStyleRef& style, uint32_t propBit, bool sheetSetsIt)
		{
			if (style.Sheet == AssetHandle(0))
				return false;

			ImGui::PushID((int)propBit);

			bool reverted = false;

			if (style.Overrides & propBit)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("x"))
				{
					style.Overrides &= ~propBit;
					reverted = true;
				}

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Overridden here. Click to revert to the stylesheet.");
			}
			else if (sheetSetsIt)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("css");

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("From the attached stylesheet. Edit to override.");
			}

			ImGui::PopID();

			return reverted;
		}

		bool StyleSheetSlot(UIStyleRef& style, Entity entity, char* nameBuffer, size_t nameBufferSize, const std::function<void(const std::filesystem::path&)>& openFileCallback)
		{
			bool changed = false;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Style Sheet");
			ImGui::TableSetColumnIndex(1);

			std::string sheetName = "None";
			if (style.Sheet != AssetHandle(0))
			{
				if (const AssetMetadata* metadata = AssetManager::GetMetadata(style.Sheet))
					sheetName = metadata->FilePath.filename().string();
				else
					sheetName = "No Style attached!";
			}

			AssetHandle pickedSheet = style.Sheet;

			const bool hasSheet = style.Sheet != AssetHandle(0);

			ImGui::PushItemWidth(hasSheet ? -125.0f : -70.0f);
			if (ImGui::BeginCombo("##stylesheet", sheetName.c_str()))
			{
				if (ImGui::Selectable("None", style.Sheet == AssetHandle(0)))
					pickedSheet = AssetHandle(0);

				AssetManager::Each(AssetType::StyleSheet, [&pickedSheet, &style](AssetHandle handle, const AssetMetadata& metadata)
					{
						const bool selected = handle == style.Sheet;

						// Handles are unique; filenames may not be if two
						// folders hold a Panel.css.
						ImGui::PushID((const void*)(uint64_t)handle);

						if (ImGui::Selectable(metadata.FilePath.filename().string().c_str(), selected))
							pickedSheet = handle;

						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("%s", metadata.FilePath.generic_string().c_str());

						if (selected)
							ImGui::SetItemDefaultFocus();

						ImGui::PopID();
					});

				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			if (pickedSheet != style.Sheet)
			{
				style.Sheet = pickedSheet;
				style.Overrides = 0;

				changed = true;
			}

			ImGui::SameLine();
			if (ImGui::Button("New..."))
			{
				strncpy_s(nameBuffer, nameBufferSize, entity.GetComponent<TagComponent>().Tag.c_str(), nameBufferSize - 1);

				ImGui::OpenPopup("Create Style Sheet");
			}

			if (hasSheet)
			{
				ImGui::SameLine();
				if (ImGui::Button("Edit"))
				{
					if (const AssetMetadata* metadata = AssetManager::GetMetadata(style.Sheet))
					{
						if (openFileCallback)
							openFileCallback(AssetManager::GetAssetDirectory() / metadata->FilePath);
					}
				}
			}

			if (ImGui::BeginPopupModal("Create Style Sheet", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("Name");
				ImGui::SetNextItemWidth(320.0f);

				// Focus the field on the first frame so the name can be typed
				// without clicking into it.
				if (ImGui::IsWindowAppearing())
					ImGui::SetKeyboardFocusHere();

				const bool submitted = ImGui::InputText("##stylesheetname", nameBuffer, nameBufferSize, ImGuiInputTextFlags_EnterReturnsTrue);

				std::string sanitised = ImGuiHelpers::SanitiseFileName(nameBuffer);

				// Show where it will land, so the folder isn't a surprise.
				auto relativePath = std::filesystem::path("Styles") / (sanitised + ".css");
				ImGui::TextDisabled("Assets/%s", relativePath.generic_string().c_str());

				const bool nameValid = !sanitised.empty();
				if (!nameValid)
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "Enter a name.");

				ImGui::Separator();

				ImGui::BeginDisabled(!nameValid);
				const bool create = ImGui::Button("Create", ImVec2(120.0f, 0.0f)) || (submitted && nameValid);
				ImGui::EndDisabled();

				if (create)
				{
					AssetHandle handle = UIStyleSystem::CreateStyleSheetFromComponent(entity, relativePath);
					if (handle != AssetHandle(0))
					{
						style.Sheet = handle;
						style.Overrides = 0;

						changed = true;
					}

					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}

			return changed;
		}

		bool TextureSlotRow(const char* label, AssetHandle currentHandle, const std::filesystem::path& assetRoot, const std::filesystem::path& browseStartDirectory, std::string& outFilepath, float thumbnailSize)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const float wrapWidth = 85.0f - ImGui::GetStyle().ItemSpacing.x;
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
			ImGui::TextUnformatted(label);
			ImGui::PopTextWrapPos();

			ImGui::TableSetColumnIndex(1);

			Texture2D* displayTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

			if (currentHandle != AssetHandle(0))
			{
				auto tex = AssetManager::GetAsset<Texture2D>(currentHandle);
				if (tex)
					displayTexture = tex.get();
			}

			ImGui::Image(displayTexture->GetID(), { thumbnailSize, thumbnailSize });

			bool picked = false;

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					outFilepath = (assetRoot / path).string();
					picked = true;
				}

				ImGui::EndDragDropTarget();
			}

			if (ImGui::IsItemClicked())
			{
				if (auto filepath = FileDialogs::OpenFile("", browseStartDirectory.string().c_str()))
				{
					outFilepath = *filepath;
					picked = true;
				}
			}

			return picked;
		}

		bool AlignmentGrid(const char* id, TextAlignH& alignH, TextAlignV& alignV, float cellSize /*= 22.0f*/)
		{
			bool changed = false;

			ImGui::PushID(id);

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			ImGui::BeginGroup();

			for (int row = 0; row < 3; row++)
			{
				for (int col = 0; col < 3; col++)
				{
					if (col > 0)
						ImGui::SameLine(0.0f, 2.0f);

					const bool selected = ((int)alignV == row && (int)alignH == col);

					ImGui::PushID(row * 3 + col);
					ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.26f, 0.59f, 0.98f, 1.0f) : ImVec4(0.18f, 0.18f, 0.18f, 1.0f));

					if (ImGui::Button("##cell", ImVec2(cellSize, cellSize)))
					{
						alignH = (TextAlignH)col;
						alignV = (TextAlignV)row;
						changed = true;
					}

					ImGui::PopStyleColor();

					const ImVec2 cellMin = ImGui::GetItemRectMin();
					const ImVec2 cellMax = ImGui::GetItemRectMax();

					const float padding = 4.0f;
					const float lineGap = 3.0f;
					const float usableWidth = (cellMax.x - cellMin.x) - padding * 2.0f;
					const float blockHeight = lineGap * 2.0f;

					const float lineWidths[3] = { usableWidth, usableWidth * 0.7f, usableWidth * 0.45f };

					float blockTop = cellMin.y + padding;
					if (row == 1)
						blockTop = (cellMin.y + cellMax.y) * 0.5f - blockHeight * 0.5f;
					else if (row == 2)
						blockTop = cellMax.y - padding - blockHeight;

					for (int line = 0; line < 3; line++)
					{
						const float lineWidth = lineWidths[line];

						float lineLeft = cellMin.x + padding;
						if (col == 1)
							lineLeft = (cellMin.x + cellMax.x) * 0.5f - lineWidth * 0.5f;
						else if (col == 2)
							lineLeft = cellMax.x - padding - lineWidth;

						const float lineY = blockTop + lineGap * line;

						drawList->AddLine(ImVec2(lineLeft, lineY), ImVec2(lineLeft + lineWidth, lineY), IM_COL32(230, 230, 230, selected ? 255 : 160), 1.0f);
					}

					ImGui::PopID();
				}
			}

			ImGui::EndGroup();
			ImGui::PopID();

			return changed;
		}

	}

}