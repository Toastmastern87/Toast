#include "tpch.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"

namespace Toast {

	Scope<Renderer2D::Renderer2DData> Renderer2D::sRenderer2DData = CreateScope<Renderer2D::Renderer2DData>();

	void Renderer2D::Init()
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->UIShader = CreateRef<Shader>("assets/shaders/UI.hlsl");

		sRenderer2DData->QuadVertexBuffer = CreateRef<VertexBuffer>(sRenderer2DData->MaxVertices * sizeof(QuadVertex), sRenderer2DData->MaxVertices, 0);

		sRenderer2DData->QuadVertexBufferBase = new QuadVertex[sRenderer2DData->MaxVertices];

		uint32_t* quadIndices = new uint32_t[sRenderer2DData->MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < sRenderer2DData->MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 2;
			quadIndices[i + 2] = offset + 1;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 0;
			quadIndices[i + 5] = offset + 3;

			offset += 4;
		}

		sRenderer2DData->QuadIndexBuffer = CreateRef<IndexBuffer>(quadIndices, sRenderer2DData->MaxIndices);
		delete[] quadIndices;

		sRenderer2DData->QuadVertexPositions[0] = DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f);
		sRenderer2DData->QuadVertexPositions[1] = DirectX::XMVectorSet(1.0f, -1.0f, 0.0f, 1.0f);
		sRenderer2DData->QuadVertexPositions[2] = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f); 
		sRenderer2DData->QuadVertexPositions[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

		// Setting up the constant buffer and data buffer for the camera rendering
		sRenderer2DData->UICBuffer = ConstantBufferLibrary::Load("UI", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 12) });
		sRenderer2DData->UICBuffer->Bind();
		sRenderer2DData->UIBuffer.Allocate(sRenderer2DData->UICBuffer->GetSize());
		sRenderer2DData->UIBuffer.ZeroInitialize();
	}

	void Renderer2D::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

		delete[] sRenderer2DData->QuadVertexBufferBase;
	}

	void Renderer2D::BeginScene()
	{
		TOAST_PROFILE_FUNCTION();

		auto[width, height] = sRendererData->FinalRenderTarget->GetSize();

		sRenderer2DData->QuadIndexCount = 0;
		sRenderer2DData->QuadVertexBufferPtr = sRenderer2DData->QuadVertexBufferBase;

		float fWidth, fHeight;
		fWidth = (float)width;
		fHeight = (float)height;

		// Updating the UI data in the buffer and mapping it to the GPU
		sRenderer2DData->UIBuffer.Write((void*)&(float)fWidth, 4, 0);
		sRenderer2DData->UIBuffer.Write((void*)&(float)fHeight, 4, 4);
		sRenderer2DData->UICBuffer->Map(sRenderer2DData->UIBuffer);
	}

	void Renderer2D::EndScene()
	{
		TOAST_PROFILE_FUNCTION();

		sRendererData->FinalFramebuffer->DisableDepth();
		sRendererData->FinalFramebuffer->Bind();

		if (sRenderer2DData->QuadIndexCount == 0) 
		{
			RenderCommand::BindBackbuffer();
			RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
			sRendererData->FinalFramebuffer->EnableDepth();
			
			return; // Nothing to draw
		}

		sRenderer2DData->UIShader->Bind();

		uint32_t dataSize = (uint32_t)((uint8_t*)sRenderer2DData->QuadVertexBufferPtr - (uint8_t*)sRenderer2DData->QuadVertexBufferBase);

		sRenderer2DData->QuadVertexBuffer->SetData(sRenderer2DData->QuadVertexBufferBase, dataSize);
		sRenderer2DData->QuadVertexBuffer->Bind();
		sRenderer2DData->QuadIndexBuffer->Bind();
		RenderCommand::DrawIndexed(0, 0, sRenderer2DData->QuadIndexCount);
		
		RenderCommand::BindBackbuffer();
		RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

		sRendererData->FinalFramebuffer->EnableDepth();
	}

	void Renderer2D::SubmitQuad(const DirectX::XMMATRIX& transform, const DirectX::XMFLOAT4& color)
	{
		TOAST_PROFILE_FUNCTION();

		constexpr size_t quadVertexCount = 4;

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			DirectX::XMStoreFloat3(&sRenderer2DData->QuadVertexBufferPtr->Position, DirectX::XMVector3Transform(sRenderer2DData->QuadVertexPositions[i], transform));
			sRenderer2DData->QuadVertexBufferPtr->Color = color;
			sRenderer2DData->QuadVertexBufferPtr++;
			
		}

		sRenderer2DData->QuadIndexCount += 6;
	}


}