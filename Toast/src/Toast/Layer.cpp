#include "tpch.h"
#include "Layer.h"

namespace Toast 
{
	Layer::Layer(const std::string& debugName) 
		: mDebugName(debugName)
	{
	}

	Layer::~Layer() 
	{
	}
}