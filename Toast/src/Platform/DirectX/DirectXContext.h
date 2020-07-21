#pragma once

#include "Toast/Renderer/GraphicsContext.h"

namespace Toast 
{
	class DirectXContext : public GraphicsContext
	{
	public:
		DirectXContext(HWND windowHandle, UINT width, UINT height);

		virtual void ResizeContext(UINT width, UINT height) override;
		virtual void SwapBuffers() override;

		virtual ID3D11Device* GetDevice() override { return mDevice; }
		virtual ID3D11DeviceContext* GetDeviceContext() override { return mDeviceContext; }
		virtual ID3D11RenderTargetView* GetRenderTargetView() override { return mRenderTargetView; }
		virtual IDXGISwapChain* GetSwapChain() override { return mSwapChain; }

	private:
		void CreateRenderTarget();
		void CleanupRenderTarget();
		void SetViewport(UINT width, UINT height);

		void LogAdapterInfo();

	private:
		HWND mWindowHandle;
		UINT mHeight, mWidth;

		ID3D11Device* mDevice = NULL;
		ID3D11DeviceContext* mDeviceContext = NULL;
		IDXGISwapChain* mSwapChain = NULL;
		ID3D11RenderTargetView* mRenderTargetView = NULL;
	};
}