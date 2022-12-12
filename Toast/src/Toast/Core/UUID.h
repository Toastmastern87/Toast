#pragma once

#include "Toast/Core/Base.h"

namespace Toast {

	class UUID 
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator const uint64_t() const { return mUUID; }

	private:
		uint64_t mUUID;
	};
}

namespace std {
	template <typename T> struct hash;

	template<>
	struct hash<Toast::UUID>
	{
		std::size_t operator()(const Toast::UUID& uuid) const
		{
			return (uint64_t)uuid;
		}
	};
}