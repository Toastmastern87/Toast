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

	template<class F>
	void parallelFor(std::size_t count, F fn) {
		// Wrap the user function so that it drops mTasksLeft _after_ running:
		auto wrapped =
			[fn = std::move(fn), this](std::size_t idx) mutable {
			fn(idx);
			// one more job just finished:
			mTasksLeft.fetch_sub(1, std::memory_order_release);
			};

		// Publish the wrapped job and reset both counters:
		mJob = std::move(wrapped);
		mCounter.store(count, std::memory_order_release);   // for scheduling
		mTasksLeft.store(count, std::memory_order_release);   // for barrier

		// —————————————— scheduling loop ——————————————
		// main thread takes tickets too:
		for (;;) {
			std::size_t cur = mCounter.load(std::memory_order_acquire);
			if (cur == 0) break;
			if (mCounter.compare_exchange_weak(
				cur, cur - 1,
				std::memory_order_acquire,
				std::memory_order_relaxed))
			{
				mJob(cur - 1);   // calls wrapped(), which will decrement mTasksLeft when done
			}
		}

		// —————————————— wait for ALL jobs to finish ——————————————
		while (mTasksLeft.load(std::memory_order_acquire) != 0) {
			std::this_thread::yield();
		}
	}

	std::size_t ThreadCount() const 
	{
		return mWorkers.size();
	}

private:
	void workerLoop() {
		for (;;) {
			if (mQuit.load(std::memory_order_acquire))
				break;

			std::size_t cur = mCounter.load(std::memory_order_acquire);
			if (cur == 0) {
				std::this_thread::yield();
				continue;
			}

			if (mCounter.compare_exchange_weak(
				cur, cur - 1,
				std::memory_order_acquire,
				std::memory_order_relaxed))
			{
				mJob(cur - 1);   // same wrapped(), so it too decrements mTasksLeft
			}
		}
	}

	std::thread::id mMainId;
	std::vector<std::thread> mWorkers;
	std::atomic<std::size_t> mCounter{ 0 };
	std::atomic<std::size_t> mTasksLeft{ 0 };

	std::atomic<bool> mQuit{ false };
	std::function<void(std::size_t)> mJob;   
};