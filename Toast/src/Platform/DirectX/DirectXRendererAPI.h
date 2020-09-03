#pragma once

#include <d3d11.h>

#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	class DirectXRendererAPI : public RendererAPI
	{
	public:
		~DirectXRendererAPI() = default;

		virtual void Init() override;
		virtual void Clear(const float clearColor[4]) override;
		virtual void BindBackbuffer() override;
		virtual void DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t indexCount = 0) override;
		virtual void SwapBuffers() override;
		virtual void ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		virtual void EnableAlphaBlending() override;
		virtual void CleanUp() override;

		virtual ID3D11Device* GetDevice() { return mDevice; }
		virtual ID3D11DeviceContext* GetDeviceContext() { return mDeviceContext; }
		virtual IDXGISwapChain* GetSwapChain() { return mSwapChain; }
	private:
		void CreateBackbuffer();
		void CreateBlendStates();

		void LogAdapterInfo();

	private:
		HWND mWindowHandle;
		UINT mHeight, mWidth;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;
		IDXGISwapChain* mSwapChain = nullptr;
		ID3D11BlendState* mAlphaBlendEnabledState = nullptr;

		Ref<Framebuffer> mBackbuffer = nullptr;
	};
}