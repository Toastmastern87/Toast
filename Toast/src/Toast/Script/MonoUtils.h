#pragma once

#include <mono/metadata/object.h>

namespace Toast {

	namespace Script {
	
		std::string ConvertMonoStringToCppString(MonoString* message);
		std::string ConvertMonoObjectToString(MonoObject* obj);
	}

}