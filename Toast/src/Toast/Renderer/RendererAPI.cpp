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

		mWindowHandle = (HWND)Application::Get().GetWindow().GetNativeWindow();

		GetClientRect(mWindowHandle, &clientRect);

		mWidth = clientRect.right - clientRect.left;
		mHeight = clientRect.bottom - clientRect.top;

		TOAST_CORE_ASSERT(mWindowHandle, "Window handle is null!");

		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = mWidth;
		sd.BufferDesc.Height = mHeight;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.BufferDesc.RefreshRate.Numerator = 0;
		sd.BufferDesc.RefreshRate.Denominator = 0;
		sd.Flags = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = mWindowHandle;
		sd.SampleDesc.Count = 4;
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

		CreateRasterizerStates();
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

	void RendererAPI::SetShaderResource(D3D11_SHADER_TYPE shaderType, uint32_t bindSlot, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
	{
		if(shaderType == D3D11_VERTEX_SHADER)
			mDeviceContext->VSSetShaderResources(bindSlot, 1, srv.GetAddressOf());
		else if(shaderType == D3D11_PIXEL_SHADER)
			mDeviceContext->PSSetShaderResources(bindSlot, 1, srv.GetAddressOf());
		else if(shaderType == D3D11_COMPUTE_SHADER)
			mDeviceContext->CSSetShaderResources(bindSlot, 1, srv.GetAddressOf());
	}

	void RendererAPI::ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		mSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_UNKNOWN, 0);
	}

	void RendererAPI::SetPrimitiveTopology(PrimitiveTopology topology)
	{
		mDeviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)topology);
	}

	void RendererAPI::SetViewport(D3D11_VIEWPORT& viewport)
	{
		mDeviceContext->RSSetViewports(1, &viewport);
	}

	void RendererAPI::SetRasterizerState(Microsoft::WRL::ComPtr<ID3D11RasterizerState>& rasterizerState)
	{
		mDeviceContext->RSSetState(rasterizerState.Get());
	}

	void RendererAPI::SetRenderTargets(const std::vector<ID3D11RenderTargetView*>& colors, Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView)
	{
		mDeviceContext->OMSetRenderTargets(static_cast<UINT>(colors.size()), colors.data(), depthView.Get());
	}

	void RendererAPI::ClearRenderTargets(ID3D11RenderTargetView* colorTarget, const DirectX::XMFLOAT4& clearColor)
	{
		mDeviceContext->ClearRenderTargetView(colorTarget, reinterpret_cast<const float*>(&clearColor));
	}

	void RendererAPI::ClearRenderTargets(std::vector<ID3D11RenderTargetView*>& colorTargets, const DirectX::XMFLOAT4& clearColor)
	{
		for (auto& colorTarget : colorTargets)
			mDeviceContext->ClearRenderTargetView(colorTarget, reinterpret_cast<const float*>(&clearColor));
	}

	void RendererAPI::ClearDepthStencilView(Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView)
	{
		mDeviceContext->ClearDepthStencilView(depthView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
	}

	void RendererAPI::SetDepthStencilState(Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState)
	{
		mDeviceContext->OMSetDepthStencilState(depthStencilState.Get(), 1);
	}

	void RendererAPI::SetBlendState(Microsoft::WRL::ComPtr<ID3D11BlendState> blendState, const DirectX::XMFLOAT4& blendFactor)
	{
		mDeviceContext->OMSetBlendState(blendState.Get(), &blendFactor.x, 0xffffffff);
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

		rasterDesc.FillMode = D3D11_FILL_WIREFRAME;

		result = mDevice->CreateRasterizerState(&rasterDesc, &mWireframeRasterizerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create wireframe rasterizer state");
	}

	void RendererAPI::GetAnnotation(Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation>& annotation)
	{
		mDeviceContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(annotation.GetAddressOf()));
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
		TOAST_CORE_INFO("  Vendor: %s", vendor.c_str());
		TOAST_CORE_INFO("  Renderer: %s", videoCardDescription);
		TOAST_CORE_INFO("  Version: %s.%s.%s.%s", major.c_str(), minor.c_str(), release.c_str(), build.c_str());
	}

}