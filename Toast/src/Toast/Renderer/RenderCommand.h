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

		static void Clear(const DirectX::XMFLOAT4 clearColor)
		{
			sRendererAPI->Clear(clearColor);
		}

		static void BindBackbuffer()
		{
			sRendererAPI->BindBackbuffer();
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

		static void EnableWireframeRendering()
		{
			sRendererAPI->EnableWireframeRendering();
		}

		static void DisableWireframeRendering()
		{
			sRendererAPI->DisableWireframeRendering();
		}

		static void SetPrimitiveTopology(PrimitiveTopology topology)
		{
			sRendererAPI->SetPrimitiveTopology(topology);
		}

		static void CleanUp()
		{
			sRendererAPI->CleanUp();
		}

	public:
		static Scope<RendererAPI> sRendererAPI;
	};
}