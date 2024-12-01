#pragma once

#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	class RenderCommand 
	{
	public:
		static void Init()
		{
			sRendererAPI->Init();
		}

		static void SetViewport(D3D11_VIEWPORT& viewport)
		{
			sRendererAPI->SetViewport(viewport);
		}

		static void SetRasterizerState(Microsoft::WRL::ComPtr<ID3D11RasterizerState>& rasterizerState) 
		{
			sRendererAPI->SetRasterizerState(rasterizerState);
		}

		static void SetRenderTargets(const std::vector<ID3D11RenderTargetView*>& colors, Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView)
		{
			sRendererAPI->SetRenderTargets(colors, depthView);
		}

		static void ClearRenderTargets(ID3D11RenderTargetView* renderTarget, const DirectX::XMFLOAT4& clearColor)
		{
			sRendererAPI->ClearRenderTargets(renderTarget, clearColor);
		}

		static void ClearRenderTargets(const std::vector<ID3D11RenderTargetView*>& colorTargets, const DirectX::XMFLOAT4& clearColor)
		{
			sRendererAPI->ClearRenderTargets(colorTargets, clearColor);
		}

		static void ClearDepthStencilView(Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView)
		{
			sRendererAPI->ClearDepthStencilView(depthView);
		}

		static void SetDepthStencilState(Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState)
		{
			sRendererAPI->SetDepthStencilState(depthStencilState);
		}

		static void SetBlendState(Microsoft::WRL::ComPtr<ID3D11BlendState> blendState, const DirectX::XMFLOAT4& blendFactor = { 0.0f, 0.0f, 0.0f, 0.0f })
		{
			sRendererAPI->SetBlendState(blendState, blendFactor);
		}

		static void DrawIndexed(const uint32_t baseVertex, const uint32_t baseIndex, const uint32_t indexCount)
		{
			sRendererAPI->DrawIndexed(baseVertex, baseIndex, indexCount);
		}

		static void DrawIndexedInstanced(const uint32_t indexCountPerInstance, const uint32_t instanceCount, const uint32_t startIndexLocation, const uint32_t baseVertexLocation, const uint32_t startInstanceLocation)
		{
			sRendererAPI->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
		}

		static void Draw(uint32_t count)
		{
			sRendererAPI->Draw(count);
		}

		static void DispatchCompute(uint32_t x, uint32_t y, uint32_t z)
		{
			sRendererAPI->DispatchCompute(x, y, z);
		}

		static void SwapBuffers(bool vSync)
		{
			sRendererAPI->SwapBuffers(vSync);
		}

		static void ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			sRendererAPI->ResizeViewport(x, y, width, height);
		}

		static void SetShaderResource(D3D11_SHADER_TYPE shaderType, uint32_t bindSlot, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
		{
			sRendererAPI->SetShaderResource(shaderType, bindSlot, srv);
		}

		static void ClearShaderResources()
		{
			sRendererAPI->ClearShaderResources();
		}

		static void SetPrimitiveTopology(PrimitiveTopology topology)
		{
			sRendererAPI->SetPrimitiveTopology(topology);
		}

		static void GetAnnotation(Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation>& annotation)
		{
			sRendererAPI->GetAnnotation(annotation);
		}

		static void CleanUp()
		{
			sRendererAPI->CleanUp();
		}

	public:
		static Scope<RendererAPI> sRendererAPI;
	};
}