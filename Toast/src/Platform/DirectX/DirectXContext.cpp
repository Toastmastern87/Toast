#include "tpch.h"
#include "DirectXContext.h"

namespace Toast
{
	DirectXContext::DirectXContext(HWND windowHandle)
		: mWindowHandle(windowHandle)
	{
		TOAST_CORE_ASSERT(mWindowHandle, "Window handle is null!");
	}

	void DirectXContext::Init()
	{
		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = mWindowHandle;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

		HRESULT result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &mSwapChain, &mD3dDevice, &featureLevel, &mD3dDeviceContext);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create DirectX device and swapchain");

		CreateRenderTarget();
	}

	void DirectXContext::SwapBuffers()
	{
		const float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

		mD3dDeviceContext->OMSetRenderTargets(1, &mRenderTargetView, NULL);
		mD3dDeviceContext->ClearRenderTargetView(mRenderTargetView, clear_color);

		mSwapChain->Present(0, 0);
	}

	void DirectXContext::CreateRenderTarget()
	{
		ID3D11Texture2D* backBuffer;
		mSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		mD3dDevice->CreateRenderTargetView(backBuffer, NULL, &mRenderTargetView);
		backBuffer->Release();
	}

	void DirectXContext::CleanupRenderTarget()
	{
		if (mRenderTargetView) 
		{ 
			mRenderTargetView->Release(); 
			mRenderTargetView = NULL; 
		}
	}
}