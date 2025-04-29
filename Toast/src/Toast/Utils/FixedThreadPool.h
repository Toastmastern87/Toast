#pragma once
#include <thread>
#include <vector>
#include <atomic>
#include <functional>

class FixedThreadPool
{
public:
	explicit FixedThreadPool(std::size_t nWorkers)
		: mMainId(std::this_thread::get_id())
	{
		for (std::size_t i = 0; i < nWorkers; ++i)
			mWorkers.emplace_back([this] { workerLoop(); });
	}
	~FixedThreadPool()
	{
		mQuit = true;
		mCounter.store(0, std::memory_order_relaxed);   // unblock
		for (auto& t : mWorkers) t.join();
	}

	/* run jobs [0 .. count-1] in parallel;   this thread participates */
	template<class F>
	void parallelFor(std::size_t count, F fn)
	{
		mJob = std::move(fn);                          // store callable
		mCounter.store(count, std::memory_order_release);

		/* main thread helps ------------------------------------------- */
		for (;;)
		{
			std::size_t cur = mCounter.load(std::memory_order_acquire);
			if (cur == 0) break;                       // all tickets taken
			if (mCounter.compare_exchange_weak(
				cur, cur - 1,
				std::memory_order_acquire,
				std::memory_order_relaxed))
				mJob(cur - 1);                         // we got one
		}

		/* wait until every worker finishes its current ticket */
		while (mCounter.load(std::memory_order_acquire) != 0)
			std::this_thread::yield();
	}

private:
	std::thread::id mMainId;

	void workerLoop()
	{
		for (;;)
		{
			if (mQuit) break;                          // pool shutting down

			std::size_t cur = mCounter.load(std::memory_order_acquire);
			if (cur == 0)                              // no work right now
			{
				std::this_thread::yield();
				continue;
			}

			/* try to claim ticket cur-1 */
			if (!mCounter.compare_exchange_weak(
				cur, cur - 1,
				std::memory_order_acquire,          // success
				std::memory_order_relaxed))         // failure
				continue;                              // someone else stole it

			mJob(cur - 1);                             // run job
		}
	}
	std::vector<std::thread> mWorkers;
	std::atomic<std::size_t> mCounter{ 0 };

	std::atomic<bool> mQuit{ false };
	std::function<void(std::size_t)> mJob;   
};