#pragma once

#include "Toast/Renderer/OrthographicCamera.h"

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class Renderer2D 
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const OrthographicCamera& camera);
		static void EndScene();
		static void Flush();

		// Primitives
		static void DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color);
		static void DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color);
		static void DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture, const float tilingFactor = 1.0f, const DirectX::XMFLOAT4& tintColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		static void DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture, const float tilingFactor = 1.0f, const DirectX::XMFLOAT4& tintColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

		static void DrawRotatedQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, float rotation, const DirectX::XMFLOAT4& color);
		static void DrawRotatedQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, float rotation, const DirectX::XMFLOAT4& color);
		static void DrawRotatedQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, float rotation, const Ref<Texture2D>& texture, const float tilingFactor = 1.0f, const DirectX::XMFLOAT4& tintColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		static void DrawRotatedQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, float rotation, const Ref<Texture2D>& texture, const float tilingFactor = 1.0f, const DirectX::XMFLOAT4& tintColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

		//Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6; }
		};

		static Statistics GetStats();
		static void ResetStats();
	private:
		static void FlushAndReset();
	};
}