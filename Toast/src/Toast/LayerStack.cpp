#include "tpch.h"
#include "LayerStack.h"

namespace Toast 
{
	LayerStack::LayerStack() 
	{
	}

	LayerStack::~LayerStack() 
	{
		for (Layer* layer : mLayers) 
			delete layer;
	}

	void LayerStack::PushLayer(Layer* layer) 
	{
		mLayers.emplace(mLayers.begin() + mLayerInsertIndex, layer);
		mLayerInsertIndex++;
		layer->OnAttach();
	}

	void LayerStack::PushOverlay(Layer* overlay)
	{
		mLayers.emplace_back(overlay);
		overlay->OnAttach();
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		auto it = std::find(mLayers.begin(), mLayers.end(), layer);

		if (it != mLayers.end()) 
		{
			mLayers.erase(it);
			mLayerInsertIndex--;
			layer->OnDetach();
		}
	}

	void LayerStack::PopOverlay(Layer* overlay)
	{
		auto it = std::find(mLayers.begin(), mLayers.end(), overlay);

		if (it != mLayers.end())
		{
			mLayers.erase(it);
			overlay->OnDetach();
		}
	}
}
