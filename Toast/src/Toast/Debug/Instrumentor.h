#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <string>
#include <thread>

namespace Toast {

	using FloatingPointMicroseconds = std::chrono::duration<double, std::micro>;

	struct ProfileResult
	{
		std::string Name;
		FloatingPointMicroseconds Start;
		std::chrono::microseconds ElapsedTime;
		std::thread::id ThreadID;
	};

	struct InstrumentationSession
	{
		std::string Name;
	};

	class Instrumentor
	{
	private:
		std::mutex mMutex;
		InstrumentationSession* mCurrentSession;
		std::ofstream mOutputStream;

	public:
		Instrumentor()
			: mCurrentSession(nullptr)
		{
		}

		void BeginSession(const std::string& name, const std::string& filepath = "results.json")
		{
			std::lock_guard lock(mMutex);
			if (mCurrentSession)
			{
				// If there is already a current session, then close it before beginning new one.
				// Subsequent profiling output meant for the original session will end up in the
				// newly opened session instead.  That's better than having badly formatted
				// profiling output.
				if (Log::GetCoreLogger()) // Edge case: BeginSession() might be before Log::Init()
				{
					TOAST_CORE_ERROR("Instrumentor::BeginSession('{0}') when session '{1}' already open.", name, mCurrentSession->Name);
				}
				InternalEndSession();
			}

			mOutputStream.open(filepath);

			if (mOutputStream.is_open()) 
			{
				mCurrentSession = new InstrumentationSession({ name });
				WriteHeader();
			}
			else 
			{
				if (Log::GetCoreLogger()) // Edge case: BeginSession() might be before Log::Init()
				{
					TOAST_CORE_ERROR("Instrumentor could not open results file '{0}'.", filepath);
				}
			}
		}

		void EndSession()
		{
			std::lock_guard lock(mMutex);
			InternalEndSession();
		}

		void WriteProfile(const ProfileResult& result)
		{
			std::stringstream json;

			json << std::setprecision(3) << std::fixed;
			json << ",{";
			json << "\"cat\":\"function\",";
			json << "\"dur\":" << (result.ElapsedTime.count()) << ',';
			json << "\"name\":\"" << result.Name << "\",";
			json << "\"ph\":\"X\",";
			json << "\"pid\":0,";
			json << "\"tid\":" << result.ThreadID << ",";
			json << "\"ts\":" << result.Start.count();
			json << "}";

			std::lock_guard lock(mMutex);
			if (mCurrentSession) 
			{
				mOutputStream << json.str();
				mOutputStream.flush();
			}
		}

		static Instrumentor& Get()
		{
			static Instrumentor instance;
			return instance;
		}

	private:
		void WriteHeader()
		{
			mOutputStream << "{\"otherData\": {},\"traceEvents\":[{}";
			mOutputStream.flush();
		}

		void WriteFooter()
		{
			mOutputStream << "]}";
			mOutputStream.flush();
		}

		void InternalEndSession() 
		{
			if (mCurrentSession)
			{
				WriteFooter();
				mOutputStream.close();
				delete mCurrentSession;
				mCurrentSession = nullptr;
			}
		}
	};

	class InstrumentationTimer
	{
	public:
		InstrumentationTimer(const char* name)
			: mName(name), mStopped(false)
		{
			mStartTimepoint = std::chrono::steady_clock::now();
		}

		~InstrumentationTimer()
		{
			if (!mStopped)
				Stop();
		}

		void Stop()
		{
			auto endTimepoint = std::chrono::steady_clock::now();
			auto highResStart = FloatingPointMicroseconds{ mStartTimepoint.time_since_epoch() };
			auto elapsedTime = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch() -					std::chrono::time_point_cast<std::chrono::microseconds>(mStartTimepoint).time_since_epoch();

			Instrumentor::Get().WriteProfile({ mName, highResStart, elapsedTime, std::this_thread::get_id() });

			mStopped = true;
		}
	private:
		const char* mName;
		std::chrono::time_point<std::chrono::steady_clock> mStartTimepoint;
		bool mStopped;
	};
}

namespace InstrumentorUtils {

	template <size_t N>
	struct ChangeResult
	{
		char Data[N];
	};

	template <size_t N, size_t K>
	constexpr auto CleanupOutputString(const char(&expr)[N], const char(&remove)[K])
	{
		ChangeResult<N> result = {};

		size_t srcIndex = 0;
		size_t dstIndex = 0;
		while (srcIndex < N)
		{
			size_t matchIndex = 0;
			while (matchIndex < K - 1 && srcIndex + matchIndex < N - 1 && expr[srcIndex + matchIndex] == remove[matchIndex])
				matchIndex++;
			if (matchIndex == K - 1)
				srcIndex += matchIndex;
			result.Data[dstIndex++] = expr[srcIndex] == '"' ? '\'' : expr[srcIndex];
			srcIndex++;
		}
		return result;
	}
}

#define TOAST_PROFILE 0
#if TOAST_PROFILE
#define TOAST_FUNC_SIG __FUNCSIG__

#define TOAST_PROFILE_BEGIN_SESSION(name, filepath) ::Toast::Instrumentor::Get().BeginSession(name, filepath)
#define TOAST_PROFILE_END_SESSION() ::Toast::Instrumentor::Get().EndSession()
#define TOAST_PROFILE_SCOPE(name) constexpr auto fixedName = ::Toast::InstrumentorUtils::CleanupOutputString(name, "__cdecl ");\
									::Toast::InstrumentationTimer timer##__LINE__(fixedName.Data)
#define TOAST_PROFILE_FUNCTION() TOAST_PROFILE_SCOPE(TOAST_FUNC_SIG)
#else
#define TOAST_PROFILE_BEGIN_SESSION(name, filepath)
#define TOAST_PROFILE_END_SESSION()
#define TOAST_PROFILE_SCOPE(name)
#define TOAST_PROFILE_FUNCTION()
#endif