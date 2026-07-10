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
		void OnImGuiRender(const FrameProfiler& profiler, const FrameStats& stats);
	private:
		struct ProfileNode 
		{
			FrameProfilerResult Data;
			std::vector<uint32_t> Children;
		};

		void BuildTree(const std::vector<FrameProfilerResult>& flat);
		void DrawStats(const FrameStats& stats);
		void DrawPassTree();
		void DrawNode(uint32_t nodeIndex, double frameGPUMS);
		double Smooth(const std::string& key, double value);

		std::vector<ProfileNode> mNodes;
		std::unordered_map<std::string, double> mSmoothed;
	};

}