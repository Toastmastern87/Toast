#pragma once

namespace Toast {

	typedef enum class MouseCode : uint16_t
	{
		ButtonLeft		= 1, 
		ButtonRight		= 2,
		ButtonMiddle	= 4,
		
		ButtonCancel	= 3,
		ButtonXButton1	= 5,
		ButtonXButton2	= 6,
	} Mouse;

	inline std::ostream& operator<<(std::ostream& os, MouseCode mouseCode)
	{
		os << static_cast<int32_t>(mouseCode);
		return os;
	}
}

#define TOAST_LBUTTON			::Toast::Mouse::ButtonLeft
#define TOAST_RBUTTON			::Toast::Mouse::ButtonRight
#define TOAST_CANCEL			::Toast::Mouse::ButtonCancel
#define TOAST_MBUTTON			::Toast::Mouse::ButtonMiddle
#define TOAST_XBUTTON1			::Toast::Mouse::ButtonXButton1
#define TOAST_XBUTTON2			::Toast::Mouse::ButtonXButton2