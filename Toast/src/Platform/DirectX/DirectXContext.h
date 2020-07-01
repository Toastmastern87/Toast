#pragma once

#include "Toast/Renderer/GraphicsContext.h"

namespace Toast 
{
	class DirectXContext : public GraphicsContext
	{
	public:
		DirectXContext(HWND windowHandle);

		virtual void Init() override;
		virtual void SwapBuffers() override;

		virtual ID3D11Device* GetD3D11Device() override { return mD3dDevice; }
		virtual ID3D11DeviceContext* GetD3D11DeviceContext() override { return mD3dDeviceContext; }

	private:
		void CreateRenderTarget();
		void CleanupRenderTarget();

	private:
		HWND mWindowHandle;

		ID3D11Device* mD3dDevice = NULL;
		ID3D11DeviceContext* mD3dDeviceContext = NULL;
		IDXGISwapChain* mSwapChain = NULL;
		ID3D11RenderTargetView* mRenderTargetView = NULL;
	};
}