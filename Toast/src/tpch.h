#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "Toast/Core/Log.h"

#include "Toast/Debug/Instrumentor.h"

#ifdef TOAST_PLATFORM_WINDOWS
	#include <Windows.h>
	#include <windowsx.h>
#endif