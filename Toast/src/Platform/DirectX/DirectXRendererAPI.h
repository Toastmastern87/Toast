#pragma once

#include <d3d11.h>
#include <wrl.h>

#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	class DirectXRendererAPI : public RendererAPI
	{
	public:
		~DirectXRendererAPI();

		virtual void Init() override;
		virtual void Clear(const DirectX::XMFLOAT4 clearColor) override;
		virtual void BindBackbuffer() override;
		virtual void DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t indexCount = 0) override;
		virtual void Draw(uint32_t count) override;
		virtual void SwapBuffers() override;
		virtual void ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		virtual void EnableAlphaBlending() override;
		virtual void EnableWireframeRendering() override;
		virtual void DisableWireframeRendering() override;
		virtual void CleanUp() override;

		virtual ID3D11Device* GetDevice() { return mDevice.Get(); }
		virtual ID3D11DeviceContext* GetDeviceContext() { return mDeviceContext.Get(); }
		virtual IDXGISwapChain* GetSwapChain() { return mSwapChain.Get(); }
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