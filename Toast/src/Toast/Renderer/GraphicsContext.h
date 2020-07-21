#pragma once

#include <d3d11.h>

namespace Toast 
{
	class GraphicsContext 
	{
	public:
		virtual void ResizeContext(UINT width, UINT height) = 0;
		virtual void SwapBuffers() = 0;

		virtual ID3D11RenderTargetView* GetRenderTargetView() = 0;
		virtual ID3D11Device* GetDevice() = 0;
		virtual ID3D11DeviceContext* GetDeviceContext() = 0;
		virtual IDXGISwapChain* GetSwapChain() = 0;

		static GraphicsContext* Create(HWND windowHandle, UINT width, UINT height);
	};
}