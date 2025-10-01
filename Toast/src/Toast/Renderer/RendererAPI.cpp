#include "tpch.h"
#include "Toast/Renderer/RendererAPI.h"

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

		UINT createDeviceFlags = 0;
#ifdef TOAST_DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_1;
		HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, &fl, 1, D3D11_SDK_VERSION, &mDevice, nullptr, &mDeviceContext);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create D3D11 device");

		TOAST_CORE_ASSERT(mWindowHandle, "Window handle is null!");

		// --- Get factory (DXGI 1.2+) ---
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		result = mDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
		TOAST_CORE_ASSERT(SUCCEEDED(result), "No IDXGIDevice");

		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		result = dxgiDevice->GetAdapter(&adapter);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "No IDXGIAdapter");

		Microsoft::WRL::ComPtr<IDXGIFactory2> factory2;
		result = adapter->GetParent(IID_PPV_ARGS(&factory2));
		TOAST_CORE_ASSERT(SUCCEEDED(result), "No IDXGIFactory2");

		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
		swapDesc.Width = mWidth;
		swapDesc.Height = mHeight;
		swapDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		swapDesc.Stereo = FALSE;
		swapDesc.SampleDesc = { 1, 0 };
		swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.BufferCount = 3;
		swapDesc.Scaling = DXGI_SCALING_STRETCH;
		swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapDesc.Flags = 0;

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain1;
		result = factory2->CreateSwapChainForHwnd(mDevice.Get(), mWindowHandle, &swapDesc, nullptr, nullptr, &swapchain1);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "CreateSwapChainForHwnd failed");

		// Disable Alt+Enter (optional, recommended for tools)
		factory2->MakeWindowAssociation(mWindowHandle, DXGI_MWA_NO_ALT_ENTER);

		Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain3;
		result = swapchain1.As(&swapchain3);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "SwapChain v3 query failed");

		UINT support = 0;
		result = swapchain3->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, &support);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "CheckColorSpaceSupport failed");

		if (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
		{
			result = swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709); // scRGB
			TOAST_CORE_ASSERT(SUCCEEDED(result), "SetColorSpace1(scRGB) failed");
		}
		else
		{
			// Fallback to SDR if OS/monitor doesn't support scRGB
			result = swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "SetColorSpace1(SDR) failed");
		}

		mSwapChain = swapchain3.Get();

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

	void RendererAPI::ClearShaderResources()
	{
		ID3D11ShaderResourceView* nullSRVs[16] = { nullptr };
		mDeviceContext->VSSetShaderResources(0, 16, nullSRVs);
		mDeviceContext->PSSetShaderResources(0, 16, nullSRVs);
		mDeviceContext->CSSetShaderResources(0, 16, nullSRVs);
	}

	void RendererAPI::CopyResource(ID3D11Resource* dest, ID3D11Resource* src)
	{
		mDeviceContext->CopyResource(dest, src);
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

	void RendererAPI::ClearRenderTargets(const std::vector<ID3D11RenderTargetView*>& colorTargets, const DirectX::XMFLOAT4& clearColor)
	{
		for (auto& colorTarget : colorTargets)
			mDeviceContext->ClearRenderTargetView(colorTarget, reinterpret_cast<const float*>(&clearColor));
	}

	void RendererAPI::ClearUAV(ID3D11UnorderedAccessView* uavTarget, const DirectX::XMFLOAT4& clearColor)
	{
		mDeviceContext->ClearUnorderedAccessViewFloat(uavTarget, &clearColor.x);
	}

	void RendererAPI::ClearUAV(ID3D11UnorderedAccessView* uavTarget, const UINT(&clearColor)[4])
	{
		mDeviceContext->ClearUnorderedAccessViewUint(uavTarget, clearColor);
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