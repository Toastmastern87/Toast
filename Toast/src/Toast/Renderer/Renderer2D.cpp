#include "tpch.h"
#include "Renderer2D.h"

#include "Shader.h"
#include "Buffer.h"
#include "RenderCommand.h"

namespace Toast {

	struct Renderer2DStorage
	{
		Ref<Shader> TextureShader;
		Ref<Texture2D> WhiteTexture;
		Ref<BufferLayout> QuadBufferLayout;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;
	};

	static Renderer2DStorage* sData;

	void Renderer2D::Init()
	{
		sData = new Renderer2DStorage();

		float vertices[5* 4] = {
								-0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
								0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
								0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
								-0.5f,  0.5f, 0.0f, 0.0f, 0.0f
		};

		sData->QuadVertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices), uint32_t(4)));

		uint32_t indices[6] = { 0, 2, 1, 2, 0, 3 };

		sData->QuadIndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));

		sData->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		sData->WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		sData->TextureShader = Shader::Create("assets/shaders/Texture.hlsl");

		const std::initializer_list<BufferLayout::BufferElement>& layout = {
																   { ShaderDataType::Float3, "POSITION" },
																   { ShaderDataType::Float2, "TEXCOORD" }
		};

		sData->QuadBufferLayout.reset(BufferLayout::Create(layout, sData->TextureShader));
	}

	void Renderer2D::Shutdown()
	{
		delete sData;
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		sData->TextureShader->Bind();
		sData->TextureShader->SetSceneData(camera.GetViewProjectionMatrix());
	}

	void Renderer2D::EndScene()
	{

	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
	{
		DrawQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, color);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
	{
		sData->TextureShader->SetColorData(DirectX::XMFLOAT4(color.x, color.y, color.z, color.w));
		sData->WhiteTexture->Bind();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(size.x, size.y, 1.0f) * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		sData->TextureShader->SetObjectData(transform);

		sData->QuadBufferLayout->Bind();
		sData->QuadVertexBuffer->Bind();
		sData->QuadIndexBuffer->Bind();
		RenderCommand::DrawIndexed(sData->QuadIndexBuffer);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture)
	{
		DrawQuad(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f), size, texture);
	}

	void Renderer2D::DrawQuad(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const Ref<Texture2D>& texture)
	{
		sData->TextureShader->SetColorData(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		texture->Bind();

		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z) * DirectX::XMMatrixScaling(size.x, size.y, 1.0f);
		sData->TextureShader->SetObjectData(transform);

		sData->QuadBufferLayout->Bind();
		sData->QuadVertexBuffer->Bind();
		sData->QuadIndexBuffer->Bind();
		RenderCommand::DrawIndexed(sData->QuadIndexBuffer);
	}
}