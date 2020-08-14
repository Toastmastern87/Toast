#include "tpch.h"
#include "Renderer2D.h"

#include "Shader.h"
#include "Buffer.h"
#include "RenderCommand.h"

#include "Platform/DirectX/DirectXShader.h"

namespace Toast {

	struct Renderer2DStorage
	{
		Ref<Shader> FlatColorShader;
		Ref<BufferLayout> QuadBufferLayout;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;
	};

	static Renderer2DStorage* sData;

	void Renderer2D::Init()
	{
		sData = new Renderer2DStorage();

		float vertices[3 * 4] = {
					-0.5f, -0.5f, 0.0f,
					0.5f, -0.5f, 0.0f,
					0.5f,  0.5f, 0.0f,
					-0.5f,  0.5f, 0.0f
		};

		sData->QuadVertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices), uint32_t(4)));

		uint32_t indices[6] = { 0, 2, 1, 2, 0, 3 };

		sData->QuadIndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));

		sData->FlatColorShader = Shader::Create("assets/shaders/FlatColor.hlsl");

		const std::initializer_list<BufferLayout::BufferElement>& layout = {
																   { ShaderDataType::Float3, "POSITION" }
		};

		sData->QuadBufferLayout.reset(BufferLayout::Create(layout, sData->FlatColorShader));
	}

	void Renderer2D::Shutdown()
	{
		delete sData;
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		std::static_pointer_cast<DirectXShader>(sData->FlatColorShader)->Bind();
		std::static_pointer_cast<DirectXShader>(sData->FlatColorShader)->UploadSceneDataVSCBuffer(camera.GetViewProjectionMatrix());
		std::static_pointer_cast<DirectXShader>(sData->FlatColorShader)->UploadObjectDataVSCBuffer(DirectX::XMMatrixIdentity());
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
		std::static_pointer_cast<DirectXShader>(sData->FlatColorShader)->Bind();
		std::static_pointer_cast<Toast::DirectXShader>(sData->FlatColorShader)->UploadColorDataPSCBuffer(DirectX::XMFLOAT4(color.x, color.y, color.z, color.w));

		sData->QuadBufferLayout->Bind();
		sData->QuadVertexBuffer->Bind();
		sData->QuadIndexBuffer->Bind();
		RenderCommand::DrawIndexed(sData->QuadIndexBuffer);
	}
}