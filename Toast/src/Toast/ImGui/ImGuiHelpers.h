#pragma once

#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Vector.h"

#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_internal.h"

#include <string>
#include <float.h> 
#include <cfloat>

namespace Toast
{
	namespace ImGuiHelpers
	{
#define STYLE_MARKER_WIDTH 34.0f

		bool ManualDragFloat(const char* label, float& value, WindowsWindow* window, std::string& activeDragArea, float speed = 0.1f, ImVec2 dragAreaSize = { 10.0f, 10.0f }, const char* displayFormat = "%.1f", float minVal = -DBL_MAX, float maxVal = FLT_MAX);
		bool ManualDragFloat2(const std::string& label, DirectX::XMFLOAT2& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea, const char* displayFormat = "%.1f", bool colorValues = false, float overrideTotalWidth = 0.0f);
		bool ManualDragFloat3(const std::string& label, DirectX::XMFLOAT3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea, const char* displayFormat = "%.1f", bool colorValues = false, float overrideTotalWidth = 0.0f);
		bool ManualDragFloat3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea, const char* displayFormat = "%.1f", bool colorValues = false);

		bool ManualDragDouble(const char* label, double& value, WindowsWindow* window, std::string& activeDragArea, float speed = 0.1f, ImVec2 dragAreaSize = { 10.0f, 10.0f }, const char* displayFormat = "%.1f", double minVal = -DBL_MAX, double maxVal = DBL_MAX);
		bool ManualDragDouble3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea);

		bool TinyExponentCombo(const char* id, int& exp10);
		bool ManualDragFloat3Scaled(const std::string& label, DirectX::XMFLOAT3& stored, int exp10, float speedMantissa, float resetStored, WindowsWindow* window, std::string& activeDragArea, const char* displayFormat, bool colorValues, float overrideTotalWidth = 0.0f);

		bool DragInt16(const char* label, int16_t* value, float speed, int min, int max);

		std::string SanitiseFileName(const std::string& input);

		bool StyleOverrideMarker(UIStyleRef& style, uint32_t propBit, bool sheetSetsIt);
		bool StyleSheetSlot(UIStyleRef& style, Entity entity, char* nameBuffer, size_t nameBufferSize, const std::function<void(const std::filesystem::path&)>& openFileCallback);

		bool TextureSlotRow(const char* label, AssetHandle currentHandle, const std::filesystem::path& assetRoot, const std::filesystem::path& browseStartDirectory, std::string& outFilepath, float thumbnailSize = 64.0f);

		bool AlignmentGrid(const char* id, TextAlignH& alignH, TextAlignV& alignV, float cellSize = 22.0f);
	}
}