#include <Toast.h>

class Mars : public Toast::Application 
{
public:
	Mars()
	{
	}

	~Mars()
	{
	}
};

Toast::Application* Toast::CreateApplication() 
{
	return new Mars();
}