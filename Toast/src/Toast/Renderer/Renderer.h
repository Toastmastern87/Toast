#pragma once

namespace Toast {

	enum class RendererAPI
	{
		None = 0, DirectX = 1
	};

	class Renderer 
	{
	public:
		inline static RendererAPI GetAPI() { return sRendererAPI; }

	private:
		static RendererAPI sRendererAPI;
	};
}