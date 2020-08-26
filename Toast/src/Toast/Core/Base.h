#pragma once

#include <memory>

#ifdef TOAST_DEBUG
	#if defined(TOAST_PLATFORM_WINDOWS)
		#define TOAST_DEBUGBREAK() __debugbreak()
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif

	#define TOAST_ENABLE_ASSERTS
#else
	#define TOAST_DEBUGBREAK()
#endif

#ifdef TOAST_ENABLE_ASSERTS
	#define TOAST_ASSERT(x, ...) { if(!(x)) { TOAST_ERROR("Assertion Failed: {0}", __VA_ARGS__); TOAST_DEBUGBREAK(); } } 
	#define TOAST_CORE_ASSERT(x, ...) { if(!(x)) { TOAST_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); TOAST_DEBUGBREAK(); } } 
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