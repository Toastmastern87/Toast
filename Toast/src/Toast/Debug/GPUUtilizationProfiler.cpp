#include "tpch.h"
#include "GPUUtilizationProfiler.h"

#include <Pdh.h>
#include "PdhMsg.h"

namespace Toast {

	bool GPUUtilizationProfiler::Init()
	{
		PDH_HQUERY query = nullptr;
		if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
			return false;

		PDH_HCOUNTER counter = nullptr;
		if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &counter) != ERROR_SUCCESS)
		{
			PdhCloseQuery(query);
			return false;
		}

		mQuery = query;
		mCounter = counter;
		mPidTag = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";

		PdhCollectQueryData(query);

		mRunning = true;
		mThread = std::thread(&GPUUtilizationProfiler::WorkerLoop, this);
		return true;
	}

	void GPUUtilizationProfiler::Shutdown()
	{
		mRunning = false;
		if (mThread.joinable())
			mThread.join();

		if(mQuery)
			PdhCloseQuery(mQuery);

		mQuery = nullptr;
		mCounter = nullptr;
		mUtilization = -1.0;
	}

	void GPUUtilizationProfiler::WorkerLoop()
	{
		while (mRunning)
		{
			for(int waited = 0; waited < SAMPLE_INTERVAL_MS && mRunning; waited += 50)
				std::this_thread::sleep_for(std::chrono::milliseconds(50));

			if (mRunning)
				Sample();
		}
	}

	void GPUUtilizationProfiler::Sample()
	{
		if (PdhCollectQueryData((PDH_HQUERY)mQuery) != ERROR_SUCCESS)
			return;

		DWORD bufferSize = 0;
		DWORD itemCount = 0;
		PDH_STATUS status = PdhGetFormattedCounterArrayW((PDH_HCOUNTER)mCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, nullptr);

		if (status != PDH_MORE_DATA)
			return;

		mBuffer.resize(bufferSize);
		auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(mBuffer.data());

		status = PdhGetFormattedCounterArrayW((PDH_HCOUNTER)mCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items);
		if (status != ERROR_SUCCESS)
			return;

		double busiest = 0.0;
		for (DWORD i = 0; i < itemCount; ++i)
		{
			if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
				continue;

			if (wcsstr(items[i].szName, mPidTag.c_str()) == nullptr)
				continue;

			const double value = items[i].FmtValue.doubleValue;
			if(value > busiest)
				busiest = value;
		}

		mUtilization = busiest < 100.0 ? busiest : 100.0;
	}

}