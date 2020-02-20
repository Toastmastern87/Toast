#include "tpch.h"
#include "LayerStack.h"

namespace Toast 
{
	LayerStack::LayerStack() 
	{
		mLayerInsert = mLayers.begin();
	}

	LayerStack::~LayerStack() 
	{
		for (Layer* layer : mLayers) 
			delete layer;
	}

	void LayerStack::PushLayer(Layer* layer) 
	{
		mLayerInsert = mLayers.emplace(mLayerInsert, layer);
	}

	void LayerStack::PushOverlay(Layer* overlay)
	{
		mLayers.emplace_back(overlay);
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		auto it = std::find(mLayers.begin(), mLayers.end(), layer);

		if (it != mLayers.end()) 
		{
			mLayers.erase(it);
			mLayerInsert--;
		}
	}

	void LayerStack::PopOverlay(Layer* overlay)
	{
		auto it = std::find(mLayers.begin(), mLayers.end(), overlay);

		if (it != mLayers.end())
			mLayers.erase(it);
	}
}
