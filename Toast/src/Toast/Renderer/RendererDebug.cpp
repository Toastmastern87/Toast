#include "tpch.h"
#include "RendererDebug.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"

using namespace DirectX;

namespace Toast {
	
	struct LineVertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT4 Color;
	};

	struct RendererDebugData
	{
		static const uint32_t MaxLines = 20000;
		static const uint32_t MaxVertices = MaxLines * 2;

		uint32_t LineVertexCount = 0;

		Ref<Shader> DebugShader;
		Ref<ShaderLayout::ShaderInputElement> LineShaderInputLayout;
		Ref<VertexBuffer> LineVertexBuffer;

		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;
	};

	static RendererDebugData sData;

	Scope<RendererDebug::SceneData> RendererDebug::mSceneData = CreateScope<RendererDebug::SceneData>();

	void RendererDebug::Init()
	{
		TOAST_PROFILE_FUNCTION();

		sData.LineVertexBuffer = CreateRef<VertexBuffer>(sData.MaxVertices * sizeof(LineVertex), sData.MaxVertices, 0);

		sData.LineVertexBufferBase = new LineVertex[sData.MaxVertices];

		sData.DebugShader = CreateRef<Shader>("assets/shaders/Debug.hlsl");

		// Setting up the constant buffer and data buffer for the debug rendering data
		mSceneData->mDebugCBuffer = ConstantBufferLibrary::Load("Camera", 208, D3D11_VERTEX_SHADER, 0);
		mSceneData->mDebugCBuffer->Bind();
		mSceneData->mDebugBuffer.Allocate(mSceneData->mDebugCBuffer->GetSize());
		mSceneData->mDebugBuffer.ZeroInitialize();
	}

	void RendererDebug::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

		delete[] sData.LineVertexBufferBase;
	}

	void RendererDebug::BeginScene(const EditorCamera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		ZeroMemory(sData.LineVertexBufferBase, sData.MaxVertices * sizeof(LineVertex));
		sData.LineVertexCount = 0;

		// Updating the camera data in the buffer and mapping it to the GPU
		mSceneData->mDebugBuffer.Write((void*)&camera.GetViewMatrix(), 64, 0);
		mSceneData->mDebugBuffer.Write((void*)&camera.GetProjection(), 64, 64);
		mSceneData->mDebugCBuffer->Map(mSceneData->mDebugBuffer);
		sData.DebugShader->Bind();

		sData.LineVertexBuffer->Bind();

		StartBatch();
	}

	void RendererDebug::EndScene()
	{
		TOAST_PROFILE_FUNCTION();

		Flush();
	}

	void RendererDebug::Flush()
	{
		if (sData.LineVertexCount == 0)
			return;

		uint32_t dataSize = (uint32_t)((uint8_t*)sData.LineVertexBufferPtr - (uint8_t*)sData.LineVertexBufferBase);
		sData.LineVertexBuffer->SetData(sData.LineVertexBufferBase, dataSize);

		RenderCommand::Draw(sData.LineVertexCount);
	}

	void RendererDebug::SubmitCameraFrustum(const SceneCamera& camera, DirectX::XMMATRIX& transform, DirectX::XMFLOAT3& pos)
	{
		DirectX::XMVECTOR forward, up, right, posVector;
		
		right = transform.r[0];
		up = transform.r[1];
		forward = transform.r[2];

		posVector = DirectX::XMLoadFloat3(&pos);

		float heightNear = 2.0f * tan(DirectX::XMConvertToRadians(camera.GetPerspectiveVerticalFOV()) / 2.0f) * camera.GetPerspectiveNearClip();
		float widthNear = heightNear * camera.GetAspecRatio();

		float heightFar= 2.0f * tan(DirectX::XMConvertToRadians(camera.GetPerspectiveVerticalFOV()) / 2.0f) * camera.GetPerspectiveFarClip();
		float widthFar = heightFar * camera.GetAspecRatio();

		DirectX::XMVECTOR centerNear = DirectX::XMLoadFloat3(&pos) + DirectX::XMVector3Normalize(forward) * camera.GetPerspectiveNearClip();
		DirectX::XMVECTOR centerFar = DirectX::XMLoadFloat3(&pos) + DirectX::XMVector3Normalize(forward) * camera.GetPerspectiveFarClip();

		DirectX::XMVECTOR nearTopLeft = centerNear + (up * (heightNear / 2.0f)) - (right * (widthNear / 2.0f));
		DirectX::XMVECTOR nearTopRight = centerNear + (up * (heightNear / 2.0f)) + (right * (widthNear / 2.0f));
		DirectX::XMVECTOR nearBottomLeft = centerNear - (up * (heightNear / 2.0f)) - (right * (widthNear / 2.0f));
		DirectX::XMVECTOR nearBottomRight = centerNear - (up * (heightNear / 2.0f)) + (right * (widthNear / 2.0f));

		DirectX::XMVECTOR farTopLeft = centerFar + (up * (heightFar / 2.0f)) - (right * (widthFar / 2.0f));
		DirectX::XMVECTOR farTopRight = centerFar + (up * (heightFar / 2.0f)) + (right * (widthFar / 2.0f));
		DirectX::XMVECTOR farBottomLeft = centerFar - (up * (heightFar / 2.0f)) - (right * (widthFar / 2.0f));
		DirectX::XMVECTOR farBottomRight = centerFar - (up * (heightFar / 2.0f)) + (right * (widthFar / 2.0f));

		RendererDebug::SubmitLine(nearTopLeft, farTopLeft);
		RendererDebug::SubmitLine(nearTopRight, farTopRight);
		RendererDebug::SubmitLine(nearBottomLeft, farBottomLeft);
		RendererDebug::SubmitLine(nearBottomRight, farBottomRight);

		RendererDebug::SubmitLine(nearTopLeft, nearTopRight);
		RendererDebug::SubmitLine(nearBottomRight, nearBottomLeft);
		RendererDebug::SubmitLine(nearBottomLeft, nearTopLeft);
		RendererDebug::SubmitLine(nearBottomRight, nearTopRight);

		RendererDebug::SubmitLine(farTopLeft, farTopRight);
		RendererDebug::SubmitLine(farBottomRight, farBottomLeft);
		RendererDebug::SubmitLine(farBottomLeft, farTopLeft);
		RendererDebug::SubmitLine(farBottomRight, farTopRight);
	}

	void RendererDebug::SubmitLine(DirectX::XMFLOAT3& p1, DirectX::XMFLOAT3& p2)
	{
		if (sData.LineVertexCount >= RendererDebugData::MaxVertices)
			NextBatch();

		sData.LineVertexBufferPtr->Position = p1;
		sData.LineVertexBufferPtr->Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		sData.LineVertexBufferPtr++;

		sData.LineVertexBufferPtr->Position = p2;
		sData.LineVertexBufferPtr->Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		sData.LineVertexBufferPtr++;

		sData.LineVertexCount += 2;
	}

	void RendererDebug::SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2)
	{
		DirectX::XMFLOAT3 p1f;
		DirectX::XMStoreFloat3(&p1f, p1);
		DirectX::XMFLOAT3 p2f;
		DirectX::XMStoreFloat3(&p2f, p2);

		RendererDebug::SubmitLine(p1f, p2f);
	}

	void RendererDebug::StartBatch()
	{
		sData.LineVertexBufferPtr = sData.LineVertexBufferBase;
	}

	void RendererDebug::NextBatch()
	{
		Flush();
		StartBatch();
	}

}