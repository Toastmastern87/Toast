#include "tpch.h"
#include "Toast/Core/Input.h"

#ifdef TOAST_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsInput.h"
#endif

namespace Toast {

	Scope<Input> Input::sInstance = Input::Create();

	Scope<Input> Input::Create()
	{
		#ifdef TOAST_PLATFORM_WINDOWS
			return CreateScope<WindowsInput>();
		#else
			TOAST_CORE_ASSERT(false, "Unkown platform!");
			return nullptr;
		#endif
	}
}