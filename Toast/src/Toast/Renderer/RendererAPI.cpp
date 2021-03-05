#include "tpch.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Framebuffer.h"

#include "Toast/Core/Application.h"

#include <d3d11.h>

namespace Toast {

	void RendererAPI::CleanUp()
	{
	}

	void RendererAPI::Init()
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
		sd.Flags = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = mWindowHandle;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		D3D_FEATURE_LEVEL featureLevels = { D3D_FEATURE_LEVEL_11_1 };

		UINT createDeviceFlags = 0;

#ifdef TOAST_DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, &featureLevels, 1, D3D11_SDK_VERSION, &sd, &mSwapChain, &mDevice, nullptr, &mDeviceContext);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create DirectX device and swapchain");

		LogAdapterInfo();

		CreateBackbuffer();

		CreateBlendStates();

		CreateRasterizerStates();

		EnableAlphaBlending();
		EnableWireframeRendering();
	}

	void RendererAPI::Clear(const DirectX::XMFLOAT4 clearColor)
	{
		mBackbuffer->Clear(clearColor);
	}

	void RendererAPI::DrawIndexed(const uint32_t baseVertex, const uint32_t baseIndex, const uint32_t indexCount)
	{
		mDeviceContext->DrawIndexed(indexCount, baseIndex, baseVertex);
	}

	void RendererAPI::DrawIndexedInstanced(const uint32_t indexCountPerInstance, const uint32_t instanceCount, const uint32_t startIndexLocation, const uint32_t baseVertexLocation, const uint32_t startInstanceLocation)
	{
		mDeviceContext->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
	}

	void RendererAPI::Draw(uint32_t count)
	{
		mDeviceContext->Draw(count, 0);
	}

	void RendererAPI::DispatchCompute(uint32_t x, uint32_t y, uint32_t z)
	{
		mDeviceContext->Dispatch(x, y, z);
	}

	void RendererAPI::SwapBuffers(bool vSync)
	{
		TOAST_PROFILE_FUNCTION();

		if (vSync)
			mSwapChain->Present(1, 0);
		else
			mSwapChain->Present(0, 0);
	}

	void RendererAPI::ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		mBackbuffer.reset();

		mSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_UNKNOWN, 0);

		CreateBackbuffer();
	}

	void RendererAPI::EnableAlphaBlending()
	{
		mDeviceContext->OMSetBlendState(mAlphaBlendEnabledState.Get(), 0, 0xffffffff);
	}

	void RendererAPI::EnableWireframeRendering()
	{
		mDeviceContext->RSSetState(mWireframeRasterizerState.Get());
	}

	void RendererAPI::DisableWireframeRendering()
	{
		mDeviceContext->RSSetState(mNormalRasterizerState.Get());
	}

	void RendererAPI::SetPrimitiveTopology(PrimitiveTopology topology)
	{
		mDeviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)topology);
	}

	void RendererAPI::BindBackbuffer()
	{
		mBackbuffer->Bind();
	}

	void RendererAPI::CreateBackbuffer()
	{
		FramebufferSpecification backbufferSpec;
		backbufferSpec.SwapChainTarget = true;
		backbufferSpec.Width = mWidth;
		backbufferSpec.Height = mHeight;
		backbufferSpec.Attachments = { FramebufferTextureFormat::R32G32B32A32_FLOAT };
		mBackbuffer = CreateRef<Framebuffer>(backbufferSpec);
	}

	void RendererAPI::CreateBlendStates()
	{
		HRESULT result;
		D3D11_BLEND_DESC bd = {};

		bd.IndependentBlendEnable = true;
		bd.RenderTarget[0].BlendEnable = true;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		bd.RenderTarget[1].BlendEnable = false;
		bd.RenderTarget[1].SrcBlend = D3D11_BLEND_ONE;
		bd.RenderTarget[1].DestBlend = D3D11_BLEND_ZERO;
		bd.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_ZERO;
		bd.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		result = mDevice->CreateBlendState(&bd, &mAlphaBlendEnabledState);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create blend states");
	}

	void RendererAPI::CreateRasterizerStates()
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

	void RendererAPI::LogAdapterInfo()
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