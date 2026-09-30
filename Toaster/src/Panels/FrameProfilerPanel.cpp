#include "FrameProfilerPanel.h"

#include "Toast/Debug/Instrumentor.h"

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

	enum class Bottleneck { Unknown, CPU, GPU, VSync };

	// if GPU utilization is above this, assume GPU bound
	static constexpr double GPU_BOUND_UTILIZATION = 90.0;
	// Present blocking for more than this share of the frame means the CPU is
	// being held back — by the GPU, or by vsync / a frame limiter.
	static constexpr double PRESENT_BLOCK_FRACTION = 0.25;

	// A pass is flagged CPU-limited when its CPU time is at least this share
	// of its GPU time
	static constexpr double CPU_LIMITED_RATIO = 0.9;
	// ...and big enough to matter (keeps tiny passes from flickering blue).
	static constexpr double CPU_LIMITED_MIN_MS = 0.5;

	static const ImVec4 COLOR_CPU(0.47f, 0.71f, 1.0f, 1.0f);
	static const ImVec4 COLOR_AMBER(0.92f, 0.75f, 0.35f, 1.0f);
	static const ImVec4 COLOR_GREEN(0.43f, 0.82f, 0.43f, 1.0f);
	static const ImVec4 COLOR_RED(0.94f, 0.39f, 0.35f, 1.0f);

	static Bottleneck ClassifyBottleneck(double frameMS, double presentMS, double gpuUtilization) 
	{
		if(frameMS <= 0.0)
			return Bottleneck::Unknown;

		const bool presentBlocks = presentMS > PRESENT_BLOCK_FRACTION * frameMS;

		if (gpuUtilization > 0.0)
		{
			if (gpuUtilization > GPU_BOUND_UTILIZATION)
				return Bottleneck::GPU;
			else if (presentBlocks)
				return Bottleneck::VSync;
			
			return Bottleneck::CPU;
		}

		return presentBlocks ? Bottleneck::GPU : Bottleneck::CPU;
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

		std::vector<std::string> pathStack;
		pathStack.push_back("");

		for (const auto& result : flat)
		{
			while (stack.size() > size_t(result.Depth) + 1)
			{
				stack.pop_back();
				pathStack.pop_back();
			}

			const std::string path = pathStack.back() + "/" + result.Name;

			// Smooth here, once, and store back into the node.
			FrameProfilerResult data = result;
			if (data.Mode != ProfileMode::CPUOnly)
				data.GPUTimeMS = Smooth(path + "#gpu", data.GPUTimeMS);
			if (data.Mode != ProfileMode::GPUOnly)
				data.CPUTimeMS = Smooth(path + "#cpu", data.CPUTimeMS);

			uint32_t nodeIndex = (uint32_t)mNodes.size();
			mNodes.push_back({ data, {} });
			mNodes[stack.back()].Children.push_back(nodeIndex);
			stack.push_back(nodeIndex);
			pathStack.push_back(path);
		}
	}

	void FrameProfilerPanel::ComputeGPUWork()
	{
		for (size_t i = mNodes.size(); i-- > 1;)
		{
			ProfileNode& node = mNodes[i];

			double childWork = 0.0;
			bool hasGPUChild = false;

			for (uint32_t childIndex : node.Children)
			{
				const ProfileNode& child = mNodes[childIndex];
				if (child.Data.Mode == ProfileMode::CPUOnly)
					continue;

				childWork += child.GPUWorkMS;
				hasGPUChild = true;
			}

			node.GPUWorkMS = hasGPUChild ? childWork : node.Data.GPUTimeMS;
		}
	}

	void FrameProfilerPanel::DrawStats(const FrameProfiler& profiler, const FrameStats& stats)
	{
		// Frame root = first real node (index 1), if the profiler has resolved yet.
		bool hasFrame = mNodes.size() > 1;
		double rendererCPU = hasFrame ? mNodes[1].Data.CPUTimeMS : 0.0;
		double presentMS = Smooth("#present", profiler.GetPresentMS());
		double flushMS = Smooth("#flush", profiler.GetFlushOverheadMS());
		double otherCPU = mFrameTimeMS - rendererCPU - presentMS;

		if(otherCPU < 0.0)
			otherCPU = 0.0;

		double gpuUtil = profiler.GetGPUUtilizationPercent();
		bool hasUtil = gpuUtil >= 0.0;
		double gpuBusyMS = hasUtil ? mFrameTimeMS * gpuUtil / 100.0 : 0.0;

		auto label = [](const char* text)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", text);
				ImGui::TableNextColumn();
			};

		auto beginTable = [](const char* id)
			{
				if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit))
					return false;
				ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch);
				return true;
			};

		bool tracing = Instrumentor::Get().IsSessionActive();
		if (ImGui::Checkbox("Chrome Tracing", &tracing))
		{
			if (tracing)
				Instrumentor::Get().BeginSession("Runtime", "ToastProfile-Runtime.json");
			else
				Instrumentor::Get().EndSession();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Records every profiled function to ToastProfile-Runtime.json\n"
				"(next to the executable). Open it in chrome://tracing\n"
				"Slows the frame down noticeably while ticked.\n"
				"Each recording overwrites the previous file.");

		SectionHeader("Frame");
		if (beginTable("##framestats"))
		{
			label("FPS");
			ImVec4 fpsColor = stats.FPS >= 60 ? COLOR_GREEN : stats.FPS >= 30 ? COLOR_AMBER : COLOR_RED;
			ImGui::TextColored(fpsColor, "%d", stats.FPS);

			label("Frame time");
			if (mFrameTimeMS > 0.0) 
				ImGui::Text("%.1f ms", mFrameTimeMS); 
			else 
				ImGui::TextDisabled("-");
	
			label("Bottleneck");
			switch (ClassifyBottleneck(mFrameTimeMS, presentMS, gpuUtil))
			{
			case Bottleneck::CPU: ImGui::TextColored(COLOR_CPU, "CPU"); break;
			case Bottleneck::GPU: ImGui::TextColored(COLOR_AMBER, "GPU"); break;
			case Bottleneck::VSync: ImGui::TextColored(COLOR_GREEN, "VSync"); break;
			default: ImGui::TextDisabled("-"); break;
			}

			ImGui::EndTable();
		}

		SectionHeader("CPU");
		if (beginTable("##cpustatis"))
		{
			label("Renderer");
			if (hasFrame)
				ImGui::Text("%.1f ms", rendererCPU);
			else
				ImGui::TextDisabled("-");

			label("Profiler");
			ImGui::Text("%.1f ms", flushMS);
			if(ImGui::IsItemHovered())
				ImGui::SetTooltip("Time spent flushing for \"Accurate GPU timing\".\nAlready included in Renderer; shown so it is not mistaken for engine work.");

			label("Present");
			ImGui::Text("%.1f ms", presentMS);

			label("Other");
			ImGui::Text("%.1f ms", otherCPU);
			if(ImGui::IsItemHovered())
				ImGui::SetTooltip("Frame time outside the renderer and Present:\nscene update, scripts, physics, editor UI.");

			ImGui::EndTable();
		}

		SectionHeader("GPU");
		if (beginTable("##gpustats"))
		{
			label("Utilization");
			if (hasUtil)
				ImGui::Text("%.0f %%", gpuUtil);
			else
				ImGui::TextDisabled("n/a");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Busiest 3D engine for this process, from Windows\n(the same counter as Task Manager's GPU graph).");

			label("Busy (est.))");
			if (hasUtil)
				ImGui::Text("%.1f ms", gpuBusyMS);
			else
				ImGui::TextDisabled("n/a");

			ImGui::EndTable();
		}

		SectionHeader("Work");
		if (beginTable("##workstats"))
		{
			label("Draw Calls");
			ImGui::Text("%s", FormatThousands(profiler.GetDrawCalls()).c_str());

			label("ConstantBuffer Maps");
			ImGui::Text("%s", FormatThousands(profiler.GetConstantBufferMaps()).c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Constant buffer updates (ConstantBuffer::Map) this frame.");

			label("Mesh Commands");
			ImGui::Text("%s", FormatThousands(profiler.GetMeshCommands()).c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Entries in the renderer's mesh draw list: one per submesh drawn.");

			ImGui::EndTable();
		}

		SectionHeader("Scene");
		if (beginTable("##scenestats"))
		{
			label("Vertices");
			ImGui::Text("%s", FormatThousands(stats.VertexCount).c_str());

			label("Hovered");
			ImGui::Text("%s", stats.HoveredEntity.c_str());

			ImGui::EndTable();
		}
	}

	void FrameProfilerPanel::DrawNode(uint32_t nodeIndex, double frameTimeMS)
	{
		const ProfileNode& node = mNodes[nodeIndex];

		bool hasGPU = node.Data.Mode != ProfileMode::CPUOnly;
		bool hasCPU = node.Data.Mode != ProfileMode::GPUOnly;
		bool leaf = node.Children.empty();

		double gpuMS = node.GPUWorkMS;   // leaf: measured; parent: sum of children
		double elapsedMS = node.Data.GPUTimeMS; // measured span, smoothed in BuildTree
		double cpuMS = node.Data.CPUTimeMS;
		double pct = (hasGPU && frameTimeMS > 0.0) ? (gpuMS / frameTimeMS) * 100.0 : 0.0;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (leaf)
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

		bool open = ImGui::TreeNodeEx(node.Data.Name.c_str(), flags);

		ImGui::TableNextColumn();
		if (hasGPU)
		{
			ImGui::Text("%.3f", gpuMS);
			if (!leaf && ImGui::IsItemHovered())
			{
				double waiting = elapsedMS > gpuMS ? elapsedMS - gpuMS : 0.0;
				ImGui::SetTooltip("GPU work (sum of child passes): %.3f ms\n"
					"Elapsed on the GPU: %.3f ms\n"
					"Waiting or unscoped: %.3f ms", gpuMS, elapsedMS, waiting);
			}
		}
		else ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (!hasCPU)
			ImGui::TextDisabled("-");
		else if (hasGPU && cpuMS > CPU_LIMITED_MIN_MS && cpuMS >= CPU_LIMITED_RATIO * gpuMS)
		{
			ImGui::TextColored(COLOR_CPU, "%.3f", cpuMS);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("CPU-limited: this pass takes at least as long on the CPU\n"
					"as its GPU work does, so the GPU ends up waiting on it.");
		}
		else
			ImGui::Text("%.3f", cpuMS);

		ImGui::TableNextColumn();
		if (hasGPU)
		{
			ImVec4 pctColor = pct < 33.0 ? COLOR_GREEN: pct < 66.0 ? COLOR_AMBER : COLOR_RED;
			ImGui::TextColored(pctColor, "%.1f%%", pct);
		}
		else
			ImGui::TextDisabled("-");

		if (open)
		{
			for (uint32_t childIndex : node.Children)
				DrawNode(childIndex, frameTimeMS);
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

		ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg
			| ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_Resizable;

		if (ImGui::BeginTable("##passtree", 4, tableFlags))
		{
			ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableSetupColumn("GPU %", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableHeadersRow();

			for (uint32_t childIndex : mNodes[0].Children)
				DrawNode(childIndex, mFrameTimeMS);

			ImGui::EndTable();
		}
	}

	void FrameProfilerPanel::OnImGuiRender(FrameProfiler& profiler, const FrameStats& stats)
	{
		ImGui::Begin(ICON_TOASTER_CALCULATOR" Profiler");   

		BuildTree(profiler.GetResults());   // build + smooth once
		ComputeGPUWork();
		mFrameTimeMS = Smooth("#frame", profiler.GetFramePeriodMS());
		DrawStats(profiler, stats);
		ImGui::Spacing();
		SectionHeader("GPU Passes");

		ImGui::TextColored(COLOR_CPU, "Blue");
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextDisabled("CPU time: the pass is CPU-limited.");

		DrawPassTree();

		ImGui::End();               
	}
} 
 
 