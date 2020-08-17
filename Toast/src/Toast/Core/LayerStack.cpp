#include "tpch.h"
#include "Toast/Core/LayerStack.h"

namespace Toast {

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
		auto it = std::find(mLayers.begin(), mLayers.begin() + mLayerInsertIndex, layer);

		if (it != mLayers.begin() + mLayerInsertIndex)
		{
			layer->OnDetach();
			mLayers.erase(it);
			mLayerInsertIndex--;
		}
	}

	void LayerStack::PopOverlay(Layer* overlay)
	{
		auto it = std::find(mLayers.begin() + mLayerInsertIndex, mLayers.end(), overlay);

		if (it != mLayers.end())
		{
			overlay->OnDetach();
			mLayers.erase(it);
		}
	}
}
