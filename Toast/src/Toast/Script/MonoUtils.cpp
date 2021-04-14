#include "tpch.h"
#include "MonoUtils.h"

namespace Toast {

	namespace Script {

		std::string ConvertMonoStringToCppString(MonoString* msg)
		{
			char* ptr = mono_string_to_utf8(msg);
			std::string s(ptr);
			mono_free(ptr);
			return s;
		}

		std::string ConvertMonoObjectToString(MonoObject* obj)
		{
			if (!obj)
				return "NULL";
			else
			{
				MonoString* a = mono_object_to_string(obj, NULL);
				return ConvertMonoStringToCppString(a);
			}
		}
	}

}