#pragma once

#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Buffer.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/Formats.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

namespace Toast {

	class RendererAPI
	{
	public:
		RendererAPI() = default;
		~RendererAPI() = default;

		void Init();
		void Clear(const DirectX::XMFLOAT4 clearColor);
		void BindBackbuffer();
		void DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t indexCount = 0);
		void Draw(uint32_t count);
		void SwapBuffers(bool vSync);
		void ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
		void EnableAlphaBlending();
		void EnableWireframeRendering();
		void DisableWireframeRendering();
		void SetPrimitiveTopology(PrimitiveTopology topology);
		void CleanUp();

		ID3D11Device* GetDevice() { return mDevice.Get(); }
		ID3D11DeviceContext* GetDeviceContext() { return mDeviceContext.Get(); }
		IDXGISwapChain* GetSwapChain() { return mSwapChain.Get(); }
	private:
		void CreateBackbuffer();
		void CreateBlendStates();
		void CreateRasterizerStates();

		void LogAdapterInfo();

	private:
		HWND mWindowHandle;
		UINT mHeight, mWidth;

		Microsoft::WRL::ComPtr<ID3D11Device> mDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
		Microsoft::WRL::ComPtr<ID3D11BlendState> mAlphaBlendEnabledState;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> mNormalRasterizerState;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> mWireframeRasterizerState;

		Ref<Framebuffer> mBackbuffer = nullptr;
	};
}