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
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = mWidth;
		sd.BufferDesc.Height = mHeight;
		sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.BufferDesc.RefreshRate.Numerator = 0;
		sd.BufferDesc.RefreshRate.Denominator = 0;
		sd.Flags = 0;// DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
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

		HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION, &sd, &mSwapChain, &mDevice, nullptr, &mDeviceContext);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create DirectX device and swapchain");

		LogAdapterInfo();

		CreateBackbuffer();

		CreateBlendStates();

		CreateRasterizerStates();

		EnableAlphaBlending();
		EnableWireframeRendering();
	}

	void DirectXRendererAPI::Clear(const DirectX::XMFLOAT4 clearColor)
	{
		mBackbuffer->Clear(clearColor);
	}

	void DirectXRendererAPI::DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t indexCount)
	{
		uint32_t count = indexCount ? indexCount : indexBuffer->GetCount();

		mDeviceContext->DrawIndexed(count, 0, 0);
	}

	void DirectXRendererAPI::Draw(uint32_t count)
	{
		mDeviceContext->Draw(count, 0);
	}

	void DirectXRendererAPI::SwapBuffers(bool vSync)
	{
		TOAST_PROFILE_FUNCTION();

		if(vSync)
			mSwapChain->Present(1, 0);
		else
			mSwapChain->Present(0, 0);
	}

	void DirectXRendererAPI::ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		mBackbuffer.reset();

		mSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_UNKNOWN, 0);

		CreateBackbuffer();
	}

	void DirectXRendererAPI::EnableAlphaBlending()
	{
		mDeviceContext->OMSetBlendState(mAlphaBlendEnabledState.Get(), 0, 0xffffffff);
	}

	void DirectXRendererAPI::EnableWireframeRendering()
	{
		mDeviceContext->RSSetState(mWireframeRasterizerState.Get());
	}

	void DirectXRendererAPI::DisableWireframeRendering()
	{
		mDeviceContext->RSSetState(mNormalRasterizerState.Get());
	}

	void DirectXRendererAPI::SetPrimitiveTopology(PrimitiveTopology topology)
	{
		mDeviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)topology);
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
		D3D11_BLEND_DESC bd = {};

		bd.RenderTarget[0].BlendEnable = true;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	
		result = mDevice->CreateBlendState(&bd, &mAlphaBlendEnabledState);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create blend states");
	}

	void DirectXRendererAPI::CreateRasterizerStates()
	{
		HRESULT result;
		D3D11_RASTERIZER_DESC rasterDesc{};

		memset(&rasterDesc, 0, sizeof(D3D11_RASTERIZER_DESC));
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.DepthClipEnable = true;

		result = mDevice->CreateRasterizerState(&rasterDesc, &mNormalRasterizerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create normal rasterizer state");

		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FillMode = D3D11_FILL_WIREFRAME;

		result = mDevice->CreateRasterizerState(&rasterDesc, &mWireframeRasterizerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create wireframe rasterizer state");
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