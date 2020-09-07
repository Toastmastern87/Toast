#include "tpch.h"
#include "Platform/DirectX/DirectXRendererAPI.h"

#include "Toast/Core/Application.h"

#include "DirectXFramebuffer.h"

#include <d3d11.h>

namespace Toast {

	void DirectXRendererAPI::CleanUp() 
	{
	}

	DirectXRendererAPI::~DirectXRendererAPI()
	{
		//ID3D11Debug* debug;
		//mDevice->QueryInterface(IID_PPV_ARGS(&debug));

		//debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

		//debug->Release();
	}

	void DirectXRendererAPI::Init()
	{
		TOAST_PROFILE_FUNCTION();

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

		HRESULT result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, mSwapChain.GetAddressOf(), mDevice.GetAddressOf(), &featureLevel, mDeviceContext.GetAddressOf());

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create DirectX device and swapchain");

		LogAdapterInfo();

		CreateBackbuffer();

		CreateBlendStates();

		mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		EnableAlphaBlending();
	}

	void DirectXRendererAPI::Clear(const float clearColor[4])
	{
		mBackbuffer->Clear(clearColor);
	}

	void DirectXRendererAPI::DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t indexCount)
	{
		uint32_t count = indexCount ? indexCount : indexBuffer->GetCount();

		mDeviceContext->DrawIndexed(count, 0, 0);
	}

	void DirectXRendererAPI::SwapBuffers()
	{
		TOAST_PROFILE_FUNCTION();

		mSwapChain->Present(0, 0);
	}

	void DirectXRendererAPI::ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		mBackbuffer.reset();

		mSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

		CreateBackbuffer();
	}

	void DirectXRendererAPI::EnableAlphaBlending()
	{
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		mDeviceContext->OMSetBlendState(mAlphaBlendEnabledState.Get(), blendFactor, 0xffffffff);
	}

	void DirectXRendererAPI::BindBackbuffer()
	{
		mBackbuffer->Bind();
	}

	void DirectXRendererAPI::CreateBackbuffer()
	{
		FramebufferSpecification backbufferSpec;
		backbufferSpec.SwapChainTarget = true;
		backbufferSpec.Width = mWidth;
		backbufferSpec.Height = mHeight;
		backbufferSpec.BuffersDesc.emplace_back(FramebufferSpecification::BufferDesc());
		mBackbuffer = Framebuffer::Create(backbufferSpec);
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

	void DirectXRendererAPI::LogAdapterInfo()
	{
		IDXGIFactory* factory = nullptr;
		IDXGIAdapter* adapter = nullptr;
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