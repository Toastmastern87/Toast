#pragma once

namespace Toast {

	using MouseCode = uint16_t;

	namespace Mouse
	{
		enum : MouseCode
		{
			ButtonLeft = 1,
			ButtonRight = 2,
			ButtonMiddle = 4
		};
	}
}