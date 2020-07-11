#pragma once

#include <d3d11.h>

namespace Toast 
{
	class GraphicsContext 
	{
	public:
		virtual void Init(UINT width, UINT height) = 0;
		virtual void StartScene() = 0;
		virtual void EndScene() = 0;
		virtual void ResizeContext(UINT width, UINT height) = 0;

		virtual ID3D11Device* GetD3D11Device() = 0;
		virtual ID3D11DeviceContext* GetD3D11DeviceContext() = 0;
	};
}