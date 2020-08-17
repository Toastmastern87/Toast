#pragma once

#include <memory>

#ifdef TOAST_PLATFORM_WINDOWS
#if TOAST_DYNAMIC_LINK
	#ifdef TOAST_BUILD_DLL
		#define TOAST_API __declspec(dllexport)
	#else
		#define TOAST_API __declspec(dllimport)
	#endif
#else
	#define TOAST_API
#endif
#else
	#error Toast only supports Windows!
#endif 

#ifdef TOAST_DEBUG
	#define TOAST_ENABLE_ASSERTS
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

#define CLEAN(x) if(x){x->Release();x=nullptr;}

namespace Toast {

	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}