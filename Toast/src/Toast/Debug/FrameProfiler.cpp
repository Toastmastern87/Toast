#include "tpch.h"
#include "FrameProfiler.h"

#include "Toast/Debug/Instrumentor.h"  
#include <thread>

namespace Toast {

	void FrameProfiler::Init(ID3D11Device* device, ID3D11DeviceContext* context) 
	{
		mDevice = device;
		mContext = context;

		D3D11_QUERY_DESC disjointDesc = {};
		disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

		D3D11_QUERY_DESC timestampsDesc = {};
		timestampsDesc.Query = D3D11_QUERY_TIMESTAMP;

		for (uint32_t f = 0; f < FRAMECOUNT; f++) 
		{
			FrameQueries& frame = mFrames[f];

			HRESULT hr = mDevice->CreateQuery(&disjointDesc, &frame.Disjoint);
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "FrameProfiler: failed to create disjoint query");

			for (uint32_t q = 0; q < MAXQUERIES; q++) 
			{
				hr = mDevice->CreateQuery(&timestampsDesc, &frame.Timestamps[q]);
				TOAST_CORE_ASSERT(SUCCEEDED(hr), "FrameProfiler: failed to create timestamp query");
			}

			frame.QueryCount = 0;
			frame.InFlight = false;
		}

		mFrameIndex = 0;

		if(!mGPUProfiler.Init())
			TOAST_CORE_WARN("FrameProfiler: failed to initialize GPU utilization profiler - the panel will show n/a.");
	}

	void FrameProfiler::Shutdown() 
	{
		mGPUProfiler.Shutdown();

		for (uint32_t f = 0; f < FRAMECOUNT; f++) 
		{
			FrameQueries& frame = mFrames[f];

			if (frame.Disjoint)
				CLEAN(frame.Disjoint);

			for (uint32_t q = 0; q < MAXQUERIES; q++)
			{
				if (frame.Timestamps[q])
					CLEAN(frame.Timestamps[q]);
			}
		}

		mDevice = nullptr;   // borrowed — clear, never Release
		mContext = nullptr;
	}

	void FrameProfiler::BeginFrame() 
	{
		if (!mEnabled)
			return;
		
		auto now = std::chrono::steady_clock::now();
		if (mLastFrameStart != std::chrono::steady_clock::time_point{})
			mFramePeriodMS = std::chrono::duration<double, std::milli>(now - mLastFrameStart).count();
		mLastFrameStart = now;

		mFlushOverheadMS = mFlushAccumMS;
		mFlushAccumMS = 0.0;

		FrameQueries& frame = mFrames[mFrameIndex];

		if (frame.InFlight)
			Resolve(frame);

		frame.QueryCount = 0;
		frame.ScopeCount = 0;
		mStackDepth = 0;

		mContext->Begin(frame.Disjoint);
		PushScope("Frame");
	}

	void FrameProfiler::EndFrame() 
	{
		if (!mEnabled)
			return;

		FrameQueries& frame = mFrames[mFrameIndex];

		PopScope();
		mContext->End(frame.Disjoint);

		frame.InFlight = true;
		mFrameIndex = (mFrameIndex + 1) % FRAMECOUNT;
	}

	void FrameProfiler::PushScope(const char* name, ProfileMode mode)
	{
		if (!mEnabled)
			return;

		FrameQueries& frame = mFrames[mFrameIndex];

		bool wantsGPU = (mode != ProfileMode::CPUOnly);

		// Graceful overflow: drop the scope, never crash. The stack only grows
		// when a scope was actually recorded, so the matching PopScope (which
		// pops only if the stack is non-empty) stays balanced automatically.
		if (frame.ScopeCount >= MAXSCOPES)
			return;
		if (wantsGPU && frame.QueryCount + 1 >= MAXQUERIES)
			return;

		ScopeRecord& scope = frame.Scopes[frame.ScopeCount];
		scope.Name = name;
		scope.Depth = mStackDepth;
		scope.Mode = mode;

		if (mode != ProfileMode::GPUOnly)
			scope.CPUStart = std::chrono::steady_clock::now();

		if (wantsGPU)
		{
			scope.BeginQuery = frame.QueryCount++;
			mContext->End(frame.Timestamps[scope.BeginQuery]);
		}

		mScopeStack[mStackDepth++] = frame.ScopeCount++;
	}

	void FrameProfiler::PopScope()
	{
		if (!mEnabled || mStackDepth == 0)
			return;

		FrameQueries& frame = mFrames[mFrameIndex];

		uint32_t index = mScopeStack[--mStackDepth];
		ScopeRecord& scope = frame.Scopes[index];

		if (scope.Mode != ProfileMode::GPUOnly)
			scope.CPUEnd = std::chrono::steady_clock::now();

		if (scope.Mode != ProfileMode::CPUOnly)
		{
			scope.EndQuery = frame.QueryCount++;
			mContext->End(frame.Timestamps[scope.EndQuery]);

			auto flushStart = std::chrono::steady_clock::now();
			mContext->Flush();
			mFlushAccumMS += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - flushStart).count();
		}

		if (scope.Mode != ProfileMode::GPUOnly) 

			Instrumentor::Get().WriteProfile({ scope.Name, FloatingPointMicroseconds{ scope.CPUStart.time_since_epoch() }, std::chrono::duration_cast<std::chrono::microseconds>(scope.CPUEnd - scope.CPUStart), std::this_thread::get_id() });
	}

	void FrameProfiler::Resolve(FrameQueries& frame)
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};

		if (mContext->GetData(frame.Disjoint, &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
			return;

		if (dj.Disjoint || dj.Frequency == 0)
			return;

		std::vector<FrameProfilerResult> results;
		results.reserve(frame.ScopeCount);

		for (uint32_t i = 0; i < frame.ScopeCount; i++)
		{
			const ScopeRecord& scope = frame.Scopes[i];

			double gpuMs = 0.0;
			if (scope.Mode != ProfileMode::CPUOnly)
			{
				UINT64 t0 = 0, t1 = 0;
				mContext->GetData(frame.Timestamps[scope.BeginQuery], &t0, sizeof(t0), D3D11_ASYNC_GETDATA_DONOTFLUSH);
				mContext->GetData(frame.Timestamps[scope.EndQuery], &t1, sizeof(t1), D3D11_ASYNC_GETDATA_DONOTFLUSH);
				gpuMs = double(t1 - t0) / double(dj.Frequency) * 1000.0;
			}

			double cpuMs = 0.0;
			if (scope.Mode != ProfileMode::GPUOnly)
				cpuMs = std::chrono::duration<double, std::milli>(scope.CPUEnd - scope.CPUStart).count();

			results.push_back({ scope.Name, scope.Depth, gpuMs, cpuMs, scope.Mode });
		}

		mResults = std::move(results);
	}
}