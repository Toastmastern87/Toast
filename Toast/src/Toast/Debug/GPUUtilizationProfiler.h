#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace Toast {

	class GPUUtilizationProfiler 
	{
	public:
		bool Init();
		void Shutdown();

		double GetUtilizationPercent() const { return mUtilization.load(); }
	private:
		void WorkerLoop();
		void Sample();
	private:
		void* mQuery = nullptr;
		void* mCounter = nullptr;

		std::wstring mPidTag;
		std::vector<unsigned char> mBuffer;

		std::thread mThread;
		std::atomic<bool> mRunning{ false };
		std::atomic<double> mUtilization{ -1.0 };

		static constexpr int SAMPLE_INTERVAL_MS = 500;
	};

}