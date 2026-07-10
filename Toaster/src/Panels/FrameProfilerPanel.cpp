#include "FrameProfilerPanel.h"

#include "../FontAwesome.h"

#include <imgui/imgui.h>

namespace Toast {

	// 1240332 -> "1,240,332"  (professional readability for big vertex counts)
	static std::string FormatThousands(uint64_t value)
	{
		std::string s = std::to_string(value);
		int insertPos = (int)s.length() - 3;
		while (insertPos > 0)
		{
			s.insert((size_t)insertPos, ",");
			insertPos -= 3;
		}
		return s;
	}

	// Stand-in for ImGui::SeparatorText (1.89+). Draws "-- Label ----------"
	// with the rule to the RIGHT of the text, which is what SeparatorText does
	// and what plain Separator()+TextDisabled() gets wrong.
	static void SectionHeader(const char* label)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("%s", label);
		ImGui::SameLine();

		// Draw a 1px rule from just after the text to the right edge, centered
		// vertically on the text.
		ImVec2 pos = ImGui::GetCursorScreenPos();
		float lineY = pos.y + ImGui::GetTextLineHeight() * 0.5f;
		float rightX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		ImU32 color = ImGui::GetColorU32(ImGuiCol_Separator);
		ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x + 4.0f, lineY), ImVec2(rightX, lineY), color);

		ImGui::NewLine();   // consume the SameLine, move below the header
		ImGui::Spacing();
	}

	double FrameProfilerPanel::Smooth(const std::string& key, double value)
	{
		auto it = mSmoothed.find(key);
		if (it == mSmoothed.end())
		{
			mSmoothed[key] = value;
			return value;
		}
		it->second = it->second * 0.9 + value * 0.1;   // EMA
		return it->second;
	}

	void FrameProfilerPanel::BuildTree(const std::vector<FrameProfilerResult>& flat)
	{
		mNodes.clear();
		mNodes.reserve(flat.size() + 1);
		mNodes.push_back({});                       // [0] synthetic root

		std::vector<uint32_t> stack;
		stack.push_back(0);

		for (const auto& result : flat)
		{
			while (stack.size() > size_t(result.Depth) + 1)
				stack.pop_back();

			// Smooth here, once, and store back into the node.
			FrameProfilerResult data = result;
			if (data.Mode != ProfileMode::CPUOnly)
				data.GPUTimeMS = Smooth(data.Name + "/gpu", data.GPUTimeMS);
			if (data.Mode != ProfileMode::GPUOnly)
				data.CPUTimeMS = Smooth(data.Name + "/cpu", data.CPUTimeMS);

			uint32_t nodeIndex = (uint32_t)mNodes.size();
			mNodes.push_back({ data, {} });
			mNodes[stack.back()].Children.push_back(nodeIndex);
			stack.push_back(nodeIndex);
		}
	}

	void FrameProfilerPanel::DrawStats(const FrameStats& stats)
	{
		// Frame root = first real node (index 1), if the profiler has resolved yet.
		bool hasFrame = mNodes.size() > 1;
		double frameGPU = hasFrame ? mNodes[1].Data.GPUTimeMS : 0.0;
		double frameCPU = hasFrame ? mNodes[1].Data.CPUTimeMS : 0.0;

		auto label = [](const char* text)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", text);
				ImGui::TableNextColumn();
			};

		SectionHeader("Frame");
		if (ImGui::BeginTable("##framestats", 2, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch);

			label("FPS");
			ImVec4 fpsColor = stats.FPS >= 60 ? ImVec4(0.43f, 0.82f, 0.43f, 1.0f)
				: stats.FPS >= 30 ? ImVec4(0.92f, 0.75f, 0.35f, 1.0f)
				: ImVec4(0.94f, 0.39f, 0.35f, 1.0f);
			ImGui::TextColored(fpsColor, "%d", stats.FPS);

			label("GPU");
			if (hasFrame) ImGui::Text("%.3f ms", frameGPU); else ImGui::TextDisabled("-");

			label("CPU");
			if (hasFrame) ImGui::Text("%.3f ms", frameCPU); else ImGui::TextDisabled("-");

			ImGui::EndTable();
		}

		SectionHeader("Scene");
		if (ImGui::BeginTable("##scenestats", 2, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch);

			label("Vertices");
			ImGui::Text("%s", FormatThousands(stats.VertexCount).c_str());

			label("Hovered");
			ImGui::Text("%s", stats.HoveredEntity.c_str());

			ImGui::EndTable();
		}
	}

	void FrameProfilerPanel::DrawNode(uint32_t nodeIndex, double frameGPUMs)
	{
		const ProfileNode& node = mNodes[nodeIndex];

		bool hasGPU = node.Data.Mode != ProfileMode::CPUOnly;
		bool hasCPU = node.Data.Mode != ProfileMode::GPUOnly;

		double gpuMs = node.Data.GPUTimeMS;   // already smoothed in BuildTree
		double cpuMs = node.Data.CPUTimeMS;
		double pct = (hasGPU && frameGPUMs > 0.0) ? (gpuMs / frameGPUMs) * 100.0 : 0.0;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
		bool leaf = node.Children.empty();
		if (leaf)
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

		bool open = ImGui::TreeNodeEx(node.Data.Name.c_str(), flags);

		ImGui::TableNextColumn();
		if (hasGPU) ImGui::Text("%.3f", gpuMs); else ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (!hasCPU)
			ImGui::TextDisabled("-");
		else if (hasGPU && cpuMs > gpuMs)          // CPU-bound: submission > execution
			ImGui::TextColored(ImVec4(0.47f, 0.71f, 1.0f, 1.0f), "%.3f", cpuMs);
		else
			ImGui::Text("%.3f", cpuMs);

		ImGui::TableNextColumn();
		if (hasGPU)
		{
			ImVec4 pctColor = pct < 33.0 ? ImVec4(0.43f, 0.82f, 0.43f, 1.0f)
				: pct < 66.0 ? ImVec4(0.92f, 0.75f, 0.35f, 1.0f)
				: ImVec4(0.94f, 0.39f, 0.35f, 1.0f);
			ImGui::TextColored(pctColor, "%.1f%%", pct);
		}
		else
			ImGui::TextDisabled("-");

		if (open)
		{
			for (uint32_t childIndex : node.Children)
				DrawNode(childIndex, frameGPUMs);
			ImGui::TreePop();
		}
	}

	void FrameProfilerPanel::DrawPassTree()
	{
		if (mNodes.size() <= 1)
		{
			ImGui::TextDisabled("Collecting...");   // first FRAME_COUNT frames
			return;
		}

		double frameGPUMs = mNodes[1].Data.GPUTimeMS;   // smoothed "Frame" root

		ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg
			| ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_Resizable;

		if (ImGui::BeginTable("##passtree", 4, tableFlags))
		{
			ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableSetupColumn("% frame", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableHeadersRow();

			for (uint32_t childIndex : mNodes[0].Children)
				DrawNode(childIndex, frameGPUMs);

			ImGui::EndTable();
		}
	}

	void FrameProfilerPanel::OnImGuiRender(const FrameProfiler& profiler, const FrameStats& stats)
	{
		ImGui::Begin(ICON_TOASTER_CALCULATOR" Profiler");   

		BuildTree(profiler.GetResults());   // build + smooth once
		DrawStats(stats);
		ImGui::Spacing();
		SectionHeader("GPU Passes");
		DrawPassTree();

		ImGui::End();               
	}
}