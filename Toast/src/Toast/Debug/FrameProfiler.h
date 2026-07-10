#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <d3d11.h>

namespace Toast {

#define TOAST_CONCAT_IMPL(a, b) a##b
#define TOAST_CONCAT(a, b) TOAST_CONCAT_IMPL(a, b)

#if TOAST_PROFILE_ENABLED
#define TOAST_PROFILE(profiler, name)     FrameProfileScope TOAST_CONCAT(frameScope, __LINE__)(profiler, name, ProfileMode::CPUAndGPU)
#define TOAST_PROFILE_GPU(profiler, name) FrameProfileScope TOAST_CONCAT(frameScope, __LINE__)(profiler, name, ProfileMode::GPUOnly)
#define TOAST_PROFILE_CPU(profiler, name) FrameProfileScope TOAST_CONCAT(frameScope, __LINE__)(profiler, name, ProfileMode:
#else
#define TOAST_PROFILE(profiler, name)
#define TOAST_PROFILE_GPU(profiler, name)
#define TOAST_PROFILE_CPU(profiler, name)
#endif

	enum class ProfileMode : uint8_t
	{
		CPUAndGPU = 0,	// default: render passes
		GPUOnly,		// pure GPU bracket, submission time uninteresting
		CPUOnly			// CPU systems (physics) — no GPU queries issued
	};

	struct FrameProfilerResult
	{
		std::string Name;
		uint32_t Depth = 0;
		double GPUTimeMS = 0.0;
		double CPUTimeMS = 0.0;
		ProfileMode Mode = ProfileMode::CPUAndGPU;
	};

	class FrameProfiler
	{
	public:
		void Init(ID3D11Device* device, ID3D11DeviceContext* context);
		void Shutdown();

		void BeginFrame();
		void EndFrame();

		void PushScope(const char* name, ProfileMode mode = ProfileMode::CPUAndGPU); 
		void PopScope();                                                             

		void SetEnabled(bool enabled) { mEnabled = enabled; }
		bool IsEnabled() { return mEnabled; }

		const std::vector<FrameProfilerResult>& GetResults() const { return mResults; }
	private:
		static constexpr uint32_t FRAMECOUNT = 3;
		static constexpr uint32_t MAXSCOPES = 64;
		static constexpr uint32_t MAXQUERIES = MAXSCOPES * 2;

		struct ScopeRecord 
		{
			const char* Name = nullptr;
			uint32_t Depth = 0;
			ProfileMode Mode = ProfileMode::CPUAndGPU;
			uint32_t BeginQuery = 0;							// unused when Mode == CPUOnly
			uint32_t EndQuery = 0;								// unused when Mode == CPUOnly
			std::chrono::steady_clock::time_point CPUStart{};   // unused when GPUOnly
			std::chrono::steady_clock::time_point CPUEnd{};
		};

		struct FrameQueries 
		{
			ID3D11Query* Disjoint = nullptr;
			ID3D11Query* Timestamps[MAXQUERIES] = {};
			ScopeRecord Scopes[MAXSCOPES];                     
			uint32_t ScopeCount = 0;
			uint32_t QueryCount = 0;
			bool InFlight = false;
		};

		void Resolve(FrameQueries& frame);

		FrameQueries mFrames[FRAMECOUNT];
		uint32_t mFrameIndex = 0;

		uint32_t mScopeStack[MAXSCOPES] = {};                
		uint32_t mStackDepth = 0;                          

		bool mEnabled = true;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mContext = nullptr;

		std::vector<FrameProfilerResult> mResults;
	};

	// RAII marker — the only thing instrumented code touches.
	class FrameProfileScope
	{
	public:
		FrameProfileScope(FrameProfiler& profiler, const char* name, ProfileMode mode = ProfileMode::CPUAndGPU)
			: mProfiler(profiler)
		{
			mProfiler.PushScope(name, mode);
		}
		~FrameProfileScope() { mProfiler.PopScope(); }
	private:
		FrameProfiler& mProfiler;
	};

}