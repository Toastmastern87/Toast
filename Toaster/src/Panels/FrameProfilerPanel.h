#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Toast/Debug/FrameProfiler.h"

namespace Toast {

	struct FrameStats 
	{
		int FPS = 0;
		uint32_t VertexCount = 0;
		std::string HoveredEntity;
	};

	class FrameProfilerPanel 
	{
	public:
		void OnImGuiRender(FrameProfiler& profiler, const FrameStats& stats);
	private:
		struct ProfileNode 
		{
			FrameProfilerResult Data;
			std::vector<uint32_t> Children;
			double GPUWorkMS = 0.0;
		};

		void BuildTree(const std::vector<FrameProfilerResult>& flat);
		void ComputeGPUWork();
		void DrawStats(const FrameProfiler& profiler, const FrameStats& stats);
		void DrawPassTree();
		void DrawNode(uint32_t nodeIndex, double frameTimeMS);
		double Smooth(const std::string& key, double value);
	private:
		std::vector<ProfileNode> mNodes;
		std::unordered_map<std::string, double> mSmoothed;

		double mFrameTimeMS = 0.0;
	};

}