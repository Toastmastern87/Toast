#include "tpch.h"
#include "Platform/DirectX/DirectXRendererAPI.h"

#include "Toast/Core/Application.h"

#include <d3d11.h>

namespace Toast {

	void DirectXRendererAPI::CleanUp() 
	{
		CLEAN(mDepthStencilView);
		CLEAN(mDepthStencilState);
		CLEAN(mDepthStencilBuffer);
		CLEAN(mAlphaBlendEnabledState);
		CLEAN(mRenderTargetView);
		CLEAN(mSwapChain);
		CLEAN(mDevice);
		CLEAN(mDeviceContext);
	}

	void DirectXRendererAPI::Init()
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_VIEWPORT viewport;
		RECT clientRect;

		mWindowHandle = Application::Get().GetWindow().GetNativeWindow();

		GetClientRect(mWindowHandle, &clientRect);

		mWidth = clientRect.right - clientRect.left;
		mHeight = clientRect.bottom - clientRect.top;

		TOAST_CORE_ASSERT(mWindowHandle, "Window handle is null!");

		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = mWidth;
		sd.BufferDesc.Height = mHeight;
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

#ifdef TOAST_DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

		HRESULT result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &mSwapChain, &mDevice, &featureLevel, &mDeviceContext);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create DirectX device and swapchain");

		LogAdapterInfo();

		CreateRenderTarget();

		CreateBlendStates();

		CreateDepthBuffer();
		CreateDepthStencil();

		viewport.TopLeftX = (float)clientRect.left;
		viewport.TopLeftY = (float)clientRect.top;
		viewport.Width = (float)mWidth;
		viewport.Height = (float)mHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		mDeviceContext->RSSetViewports(1, &viewport);

		mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		EnableDepthTesting();
		EnableAlphaBlending();
	}

	void DirectXRendererAPI::Clear(const float clearColor[4])
	{
		mDeviceContext->ClearRenderTargetView(mRenderTargetView, clearColor);
		mDeviceContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_FLAG::D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	void DirectXRendererAPI::SetRenderTargets() 
	{
		mDeviceContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);
	}

	void DirectXRendererAPI::DrawIndexed(const Ref<IndexBuffer>& indexBuffer)
	{
		mDeviceContext->DrawIndexed(indexBuffer->GetCount(), 0, 0);
	}

	void DirectXRendererAPI::SwapBuffers()
	{
		TOAST_PROFILE_FUNCTION();

		mSwapChain->Present(0, 0);
	}

	void DirectXRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		D3D11_VIEWPORT viewport;

		mWidth = width;
		mHeight = height;

		CleanupRenderTarget();
		
		ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

		viewport.TopLeftX = (float)x;
		viewport.TopLeftY = (float)y;
		viewport.Width = (float)width;
		viewport.Height = (float)height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		mDeviceContext->RSSetViewports(1, &viewport);

		mSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
		CreateRenderTarget();
		CreateDepthBuffer();
		CreateDepthStencil();
	}

	void DirectXRendererAPI::EnableAlphaBlending()
	{
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		mDeviceContext->OMSetBlendState(mAlphaBlendEnabledState, blendFactor, 0xffffffff);
	}

	void DirectXRendererAPI::EnableDepthTesting()
	{
		mDeviceContext->OMSetDepthStencilState(mDepthStencilState, 1);
	}

	void DirectXRendererAPI::CreateRenderTarget()
	{
		ID3D11Texture2D* backBuffer;
		mSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		mDevice->CreateRenderTargetView(backBuffer, NULL, &mRenderTargetView);
		backBuffer->Release();
	}

	void DirectXRendererAPI::CreateDepthBuffer()
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC depthBufferDesc;
		depthBufferDesc.Width = mWidth;
		depthBufferDesc.Height = mHeight;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.ArraySize = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags = 0;
		depthBufferDesc.MiscFlags = 0;

		result = mDevice->CreateTexture2D(&depthBufferDesc, NULL, &mDepthStencilBuffer);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create back buffer");
	}

	void DirectXRendererAPI::CreateBlendStates()
	{
		HRESULT result;
		D3D11_BLEND_DESC bd;

		bd.AlphaToCoverageEnable = false;
		bd.IndependentBlendEnable = false;
		bd.RenderTarget[0].BlendEnable = true;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		result = mDevice->CreateBlendState(&bd, &mAlphaBlendEnabledState);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create blend states");
	}

	void DirectXRendererAPI::CreateDepthStencil()
	{
		HRESULT result;
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

		// Depth test parameters
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		// Stencil test parameters
		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		// Stencil operations if pixel is front-facing
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		// Stencil operations if pixel is back-facing
		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		result = mDevice->CreateDepthStencilState(&depthStencilDesc, &mDepthStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil state");

		ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		result = mDevice->CreateDepthStencilView(mDepthStencilBuffer, &depthStencilViewDesc, &mDepthStencilView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil view");
	}

	void DirectXRendererAPI::CleanupRenderTarget()
	{
		CLEAN(mRenderTargetView);
	}

	void DirectXRendererAPI::LogAdapterInfo()
	{
		IDXGIFactory* factory = NULL;
		IDXGIAdapter* adapter = NULL;
		DXGI_ADAPTER_DESC adapterDesc;

		CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);

		factory->EnumAdapters(0, &adapter);

		adapter->GetDesc(&adapterDesc);

		char videoCardDescription[128];
		std::string vendor, major, minor, release, build;
		LARGE_INTEGER driverVersion;

		wcstombs_s(NULL, videoCardDescription, 128, adapterDesc.Description, 128);

		if (adapterDesc.VendorId == 0x10DE)
			vendor = "NVIDIA Corporation";
		else if (adapterDesc.VendorId == 0x1002)
			vendor = "AMD";
		else if (adapterDesc.VendorId == 0x8086)
			vendor = "Intel";
		else if (adapterDesc.VendorId == 0x1414)
			vendor = "Microsoft";
		else
			vendor = "Unknown vendor!";

		adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driverVersion);

		major = std::to_string(HIWORD(driverVersion.HighPart));
		minor = std::to_string(LOWORD(driverVersion.HighPart));
		release = std::to_string(HIWORD(driverVersion.LowPart));
		build = std::to_string(LOWORD(driverVersion.LowPart));

		TOAST_CORE_INFO("DirectX Information:");
		TOAST_CORE_INFO("  Vendor: {0}", vendor);
		TOAST_CORE_INFO("  Renderer: {0}", videoCardDescription);
		TOAST_CORE_INFO("  Version: {0}.{1}.{2}.{3}", major, minor, release, build);
	}
}