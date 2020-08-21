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
	};

	struct Renderer2DData
	{
		const uint32_t MaxQuads = 10000;
		const uint32_t MaxVertices = MaxQuads * 4;
		const uint32_t MaxIndices = MaxQuads * 6;

		Ref<Shader> TextureShader;
		Ref<Texture2D> WhiteTexture;
		Ref<BufferLayout> QuadBufferLayout;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;

		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;
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

		sData.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		sData.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		sData.TextureShader = Shader::Create("assets/shaders/Texture.hlsl");

		const std::initializer_list<BufferLayout::BufferElement>& layout = {
																   { ShaderDataType::Float3, "POSITION" },
																   { ShaderDataType::Float4, "COLOR" },
																   { ShaderDataType::Float2, "TEXCOORD" }
		};

		sData.QuadBufferLayout = BufferLayout::Create(layout, sData.TextureShader);
	}

	void Renderer2D::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		sData.TextureShader->Bind();
		sData.TextureShader->SetSceneData(camera.GetViewProjectionMatrix());

		sData.QuadIndexCount = 0;
		sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
	}

	void Renderer2D::EndScene()
	{
		TOAST_PROFILE_FUNCTION();

		uint32_t dataSize = (uint8_t*)sData.QuadVertexBufferPtr - (uint8_t*)sData.QuadVertexBufferBase;
		sData.QuadVertexBuffer->SetData(sData.QuadVertexBufferBase, dataSize);

		Flush();
	}

	void Renderer2D::Flush() 
	{
		RenderCommand::DrawIndexed(sData.QuadIndexBuffer, sData.QuadIndexCount);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		DrawQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, color);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		sData.QuadVertexBufferPtr->Position = pos;
		sData.QuadVertexBufferPtr->Color = color;
		sData.QuadVertexBufferPtr->TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);
		sData.QuadVertexBufferPtr++;

		sData.QuadVertexBufferPtr->Position = DirectX::XMFLOAT3(pos.x + size.x, pos.y, pos.z);
		sData.QuadVertexBufferPtr->Color = color;
		sData.QuadVertexBufferPtr->TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);
		sData.QuadVertexBufferPtr++;

		sData.QuadVertexBufferPtr->Position = DirectX::XMFLOAT3(pos.x + size.x, pos.y + size.y, pos.z);
		sData.QuadVertexBufferPtr->Color = color;
		sData.QuadVertexBufferPtr->TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);
		sData.QuadVertexBufferPtr++;

		sData.QuadVertexBufferPtr->Position = DirectX::XMFLOAT3(pos.x, pos.y + size.y, pos.z);
		sData.QuadVertexBufferPtr->Color = color;
		sData.QuadVertexBufferPtr->TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
		sData.QuadVertexBufferPtr++;

		sData.QuadIndexCount += 6;

		//sData.TextureShader->SetColorData(DirectX::XMFLOAT4(color.x, color.y, color.z, color.w), 1.0f);
		//sData.WhiteTexture->Bind();

		//DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		//sData.TextureShader->SetObjectData(transform);

		//sData.QuadBufferLayout->Bind();
		//sData.QuadVertexBuffer->Bind();
		//sData.QuadIndexBuffer->Bind();
		//RenderCommand::DrawIndexed(sData.QuadIndexBuffer);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		DrawQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		TOAST_PROFILE_FUNCTION();

		sData.TextureShader->SetColorData(tintColor, tilingFactor);
		texture->Bind();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		sData.TextureShader->SetObjectData(transform);

		sData.QuadBufferLayout->Bind();
		sData.QuadVertexBuffer->Bind();
		sData.QuadIndexBuffer->Bind();
		RenderCommand::DrawIndexed(sData.QuadIndexBuffer);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, float rotation, const DirectX::XMFLOAT4& color)
	{
		DrawRotatedQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, rotation, color);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, float rotation, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		sData.TextureShader->SetColorData(DirectX::XMFLOAT4(color.x, color.y, color.z, color.w), 1.0f);
		sData.WhiteTexture->Bind();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f)
			* DirectX::XMMatrixRotationZ(rotation) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		sData.TextureShader->SetObjectData(transform);

		sData.QuadBufferLayout->Bind();
		sData.QuadVertexBuffer->Bind();
		sData.QuadIndexBuffer->Bind();
		RenderCommand::DrawIndexed(sData.QuadIndexBuffer);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, float rotation, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		DrawRotatedQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawRotatedQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, float rotation, const Ref<Texture2D>& texture, const float tilingFactor, const DirectX::XMFLOAT4& tintColor)
	{
		TOAST_PROFILE_FUNCTION();

		sData.TextureShader->SetColorData(tintColor, tilingFactor);
		texture->Bind();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f)
			* DirectX::XMMatrixRotationZ(rotation) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		sData.TextureShader->SetObjectData(transform);

		sData.QuadBufferLayout->Bind();
		sData.QuadVertexBuffer->Bind();
		sData.QuadIndexBuffer->Bind();
		RenderCommand::DrawIndexed(sData.QuadIndexBuffer);
	}
}