#pragma once

#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/Formats.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl.h>

namespace Toast {

	class RendererAPI
	{
	public:
		RendererAPI() = default;
		~RendererAPI() = default;

		void Init();
		//void Clear(const DirectX::XMFLOAT4 clearColor);
		void DrawIndexed(const uint32_t baseVertex, const uint32_t baseIndex, const uint32_t indexCount);
		void DrawIndexedInstanced(const uint32_t indexCountPerInstance, const uint32_t instanceCount, const uint32_t startIndexLocation, const uint32_t baseVertexLocation, const uint32_t startInstanceLocation);
		void Draw(uint32_t count);
		void DispatchCompute(uint32_t x, uint32_t y, uint32_t z);
		void SwapBuffers(bool vSync);
		void SetShaderResource(D3D11_SHADER_TYPE shaderType, uint32_t bindSlot, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv);
		void ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
		void SetPrimitiveTopology(PrimitiveTopology topology);
		void CleanUp();

		void SetViewport(D3D11_VIEWPORT& viewport);
		void SetRasterizerState(Microsoft::WRL::ComPtr<ID3D11RasterizerState>& rasterizerState);
		void SetRenderTargets(const std::vector<ID3D11RenderTargetView*>& colors, Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView);
		void ClearRenderTargets(ID3D11RenderTargetView* renderTarget, const DirectX::XMFLOAT4& clearColor);
		void ClearRenderTargets(std::vector<ID3D11RenderTargetView*>& colorTargets, const DirectX::XMFLOAT4& clearColor);
		void ClearDepthStencilView(Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView);
		void SetDepthStencilState(Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState);
		void SetBlendState(Microsoft::WRL::ComPtr<ID3D11BlendState> blendState, const DirectX::XMFLOAT4& blendFactor);

		void GetAnnotation(Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation>& annotation);

		ID3D11Device* GetDevice() { return mDevice.Get(); }
		ID3D11DeviceContext* GetDeviceContext() { return mDeviceContext.Get(); }
		IDXGISwapChain* GetSwapChain() { return mSwapChain.Get(); }
	private:
		void CreateRasterizerStates();

		void LogAdapterInfo();

	private:
		HWND mWindowHandle;
		UINT mHeight, mWidth;

		Microsoft::WRL::ComPtr<ID3D11Device> mDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> mNormalRasterizerState;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> mWireframeRasterizerState;
	};
}