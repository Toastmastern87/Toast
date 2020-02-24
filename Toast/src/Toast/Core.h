#pragma once

#ifdef TOAST_PLATFORM_WINDOWS
	#ifdef TOAST_BUILD_DLL
		#define TOAST_API __declspec(dllexport)
	#else
		#define TOAST_API __declspec(dllimport)
	#endif
#else
	#error Toast only supports Windows!
#endif 

#ifdef TOAST_ENABLE_ASSERTS
	#define TOAST_ASSERT(x, ...) { if(!(x)) { TOAST_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } } 
	#define TOAST_CORE_ASSERT(x, ...) { if(!(x)) { TOAST_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } } 
#else
	#define TOAST_ASSERT(x, ...)
	#define TOAST_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define TOAST_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)