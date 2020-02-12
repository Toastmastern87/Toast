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
