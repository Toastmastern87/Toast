#pragma once

#include "Toast/Renderer/OrthographicCamera.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Camera.h"

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class Renderer2D : Renderer
	{
	private:
		struct QuadVertex
		{
			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT4 Color;
		};

		struct Renderer2DData
		{
			static const uint32_t MaxQuads = 20000;
			static const uint32_t MaxVertices = MaxQuads * 4;
			static const uint32_t MaxIndices = MaxQuads * 6;

			Ref<Shader> UIShader;

			Ref<ShaderLayout> QuadBufferLayout;
			Ref<VertexBuffer> QuadVertexBuffer;
			Ref<IndexBuffer> QuadIndexBuffer;

			uint32_t QuadIndexCount = 0;
			QuadVertex* QuadVertexBufferBase = nullptr;
			QuadVertex* QuadVertexBufferPtr = nullptr;

			DirectX::XMVECTOR QuadVertexPositions[4];

			Ref<ConstantBuffer> UICBuffer;
			Buffer UIBuffer;
		};

	protected:
		static Scope<Renderer2DData> sRenderer2DData;

	public:
		static void Init();
		static void Shutdown();

		static void BeginScene();
		static void EndScene();

		// Primitives
		static void SubmitQuad(const DirectX::XMMATRIX& transform, const DirectX::XMFLOAT4& color);

	};
}