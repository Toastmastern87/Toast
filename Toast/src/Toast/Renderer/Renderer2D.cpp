#include "tpch.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Buffer.h"
#include "Toast/Renderer/RenderCommand.h"

namespace Toast {

	struct QuadVertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT4 Color;
		DirectX::XMFLOAT2 TexCoord;
		float TexIndex;
		float TilingFactor;
	};

	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // RenderCaps

		Ref<Shader> TextureShader;
		Ref<Texture2D> WhiteTexture;
		Ref<BufferLayout> QuadBufferLayout;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;

		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1;

		DirectX::XMVECTOR QuadVertexPositions[4];

		Renderer2D::Statistics Stats;
	};

	static Renderer2DData sData;

	void Renderer2D::Init()
	{
		TOAST_PROFILE_FUNCTION();

		sData.QuadVertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(QuadVertex), sData.MaxVertices);

		sData.QuadVertexBufferBase = new QuadVertex[sData.MaxVertices];

		uint32_t* quadIndices = new uint32_t[sData.MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < sData.MaxIndices; i += 6) 
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 2;
			quadIndices[i + 2] = offset + 1;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 0;
			quadIndices[i + 5] = offset + 3;

			offset += 4;
		}

		sData.QuadIndexBuffer = IndexBuffer::Create(quadIndices, sData.MaxIndices);
		delete[] quadIndices;

		sData.WhiteTexture = Texture2D::Create(1, 1, 0);
		uint32_t whiteTextureData = 0xffffffff;
		sData.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		sData.TextureShader = Shader::Create("assets/shaders/Texture.hlsl");

		const std::initializer_list<BufferLayout::BufferElement>& layout = {
																   { ShaderDataType::Float3, "POSITION" },
																   { ShaderDataType::Float4, "COLOR" },
																   { ShaderDataType::Float2, "TEXCOORD" },
																   { ShaderDataType::Float, "PSIZE" },
																   { ShaderDataType::Float, "PSIZE", 1 }
		};

		sData.QuadBufferLayout = BufferLayout::Create(layout, sData.TextureShader);

		sData.TextureSlots[0] = sData.WhiteTexture;

		sData.QuadVertexPositions[0] = DirectX::XMVectorSet(-0.5f, -0.5f, 0.0f, 1.0f);
		sData.QuadVertexPositions[1] = DirectX::XMVectorSet(0.5f, -0.5f, 0.0f, 1.0f);
		sData.QuadVertexPositions[2] = DirectX::XMVectorSet(0.5f, 0.5f, 0.0f, 1.0f); 
		sData.QuadVertexPositions[3] = DirectX::XMVectorSet(-0.5f, 0.5f, 0.0f, 1.0f);
	}

	void Renderer2D::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

		delete[] sData.QuadVertexBufferBase;
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		sData.TextureShader->Bind();
		sData.TextureShader->SetSceneData(camera.GetViewProjectionMatrix());

		sData.QuadIndexCount = 0;
		sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;

		sData.TextureSlotIndex = 1;
	}

	void Renderer2D::EndScene()
	{
		TOAST_PROFILE_FUNCTION();

		uint32_t dataSize = (uint32_t)((uint8_t*)sData.QuadVertexBufferPtr - (uint8_t*)sData.QuadVertexBufferBase);
		sData.QuadVertexBuffer->SetData(sData.QuadVertexBufferBase, dataSize);

		Flush();
	}

	void Renderer2D::Flush() 
	{
		if (sData.QuadIndexCount == 0)
			return; // Nothing to draw

		for (uint32_t i = 0; i < sData.TextureSlotIndex; i++) 
		{
			sData.TextureSlots[i]->Bind();
		}

		RenderCommand::DrawIndexed(sData.QuadIndexBuffer, sData.QuadIndexCount);
		sData.Stats.DrawCalls++;
	}

	void Renderer2D::FlushAndReset()
	{
		EndScene();

		sData.QuadIndexCount = 0;
		sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;

		sData.TextureSlotIndex = 1;
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		DrawQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, color);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

		DrawQuad(transform, color);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		DrawQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

		DrawQuad(transform, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const DirectX::XMMATRIX& transform, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f;
		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };
		const float tilingFactor = 1.0f;

		if (sData.QuadIndexCount >= Renderer2DData::MaxIndices)
			FlushAndReset();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			DirectX::XMStoreFloat3(&sData.QuadVertexBufferPtr->Position, DirectX::XMVector3Transform(sData.QuadVertexPositions[i], transform));
			sData.QuadVertexBufferPtr->Color = color;
			sData.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			sData.QuadVertexBufferPtr->TexIndex = textureIndex;
			sData.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			sData.QuadVertexBufferPtr++;
		}

		sData.QuadIndexCount += 6;

		sData.Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const DirectX::XMMATRIX& transform, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		TOAST_PROFILE_FUNCTION();

		constexpr size_t quadVertexCount = 4;
		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };
		float textureIndex = 0.0f;

		if (sData.QuadIndexCount >= Renderer2DData::MaxIndices)
			FlushAndReset();

		for (uint32_t i = 1; i < sData.TextureSlotIndex; i++)
		{
			if (*sData.TextureSlots[i].get() == *texture.get())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0)
		{
			if (sData.TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
				FlushAndReset();

			textureIndex = (float)sData.TextureSlotIndex;
			sData.TextureSlots[sData.TextureSlotIndex] = texture;
			sData.TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			DirectX::XMStoreFloat3(&sData.QuadVertexBufferPtr->Position, DirectX::XMVector3Transform(sData.QuadVertexPositions[i], transform));
			sData.QuadVertexBufferPtr->Color = tintColor;
			sData.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			sData.QuadVertexBufferPtr->TexIndex = textureIndex;
			sData.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			sData.QuadVertexBufferPtr++;
		}

		sData.QuadIndexCount += 6;

		sData.Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, float rotation, const DirectX::XMFLOAT4& color)
	{
		DrawRotatedQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, rotation, color);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, float rotation, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f)
			* DirectX::XMMatrixRotationZ(rotation) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

		DrawRotatedQuad(transform, color);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, float rotation, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		DrawRotatedQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, float rotation, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f)
			* DirectX::XMMatrixRotationZ(rotation) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

		DrawRotatedQuad(transform, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMMATRIX& transform, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f;
		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };
		const float tilingFactor = 1.0f;

		if (sData.QuadIndexCount >= Renderer2DData::MaxIndices)
			FlushAndReset();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			DirectX::XMStoreFloat3(&sData.QuadVertexBufferPtr->Position, DirectX::XMVector3Transform(sData.QuadVertexPositions[i], transform));
			sData.QuadVertexBufferPtr->Color = color;
			sData.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			sData.QuadVertexBufferPtr->TexIndex = textureIndex;
			sData.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			sData.QuadVertexBufferPtr++;
		}

		sData.QuadIndexCount += 6;

		sData.Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMMATRIX& transform, const Ref<Texture2D>& texture, const float tilingFactor /*= 1.0f*/, const DirectX::XMFLOAT4& tintColor /*= DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)*/)
	{
		TOAST_PROFILE_FUNCTION();

		constexpr size_t quadVertexCount = 4;
		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };
		float textureIndex = 0.0f;

		if (sData.QuadIndexCount >= Renderer2DData::MaxIndices)
			FlushAndReset();

		for (uint32_t i = 1; i < sData.TextureSlotIndex; i++)
		{
			if (*sData.TextureSlots[i].get() == *texture.get())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0)
		{
			if (sData.TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
				FlushAndReset();

			textureIndex = (float)sData.TextureSlotIndex;
			sData.TextureSlots[sData.TextureSlotIndex] = texture;
			sData.TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			DirectX::XMStoreFloat3(&sData.QuadVertexBufferPtr->Position, DirectX::XMVector3Transform(sData.QuadVertexPositions[i], transform));
			sData.QuadVertexBufferPtr->Color = tintColor;
			sData.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			sData.QuadVertexBufferPtr->TexIndex = textureIndex;
			sData.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			sData.QuadVertexBufferPtr++;
		}

		sData.QuadIndexCount += 6;

		sData.Stats.QuadCount++;
	}

	void Renderer2D::ResetStats()
	{
		memset(&sData.Stats, 0, sizeof(Statistics));
	}

	Renderer2D::Statistics Renderer2D::GetStats()
	{
		return sData.Stats;
	}
}