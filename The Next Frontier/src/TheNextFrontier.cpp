#include <Toast.h>

class TheNextFrontier : public Toast::Application 
{
public:
	TheNextFrontier() 
	{
	}

	~TheNextFrontier() 
	{
	}
};

Toast::Application* Toast::CreateApplication() 
{
	return new TheNextFrontier();
}