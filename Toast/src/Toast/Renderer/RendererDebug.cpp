#include "tpch.h"
#include "RendererDebug.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"

using namespace DirectX;

namespace Toast {

	Scope<RendererDebug::DebugData> RendererDebug::mDebugData = CreateScope<RendererDebug::DebugData>();

	void RendererDebug::Init()
	{
		TOAST_PROFILE_FUNCTION();

		mDebugData->LineVertexBuffer = CreateRef<VertexBuffer>(mDebugData->MaxVertices * sizeof(LineVertex), mDebugData->MaxVertices, 0);

		mDebugData->LineVertexBufferBase = new LineVertex[mDebugData->MaxVertices];

		mDebugData->DebugShader = CreateRef<Shader>("assets/shaders/Debug.hlsl");
		mDebugData->GridShader = CreateRef<Shader>("assets/shaders/Grid.hlsl");

		// Setting up the constant buffer and data buffer for the debug rendering data
		mDebugData->mDebugCBuffer = ConstantBufferLibrary::Load("Camera", 272, D3D11_VERTEX_SHADER, 0);
		mDebugData->mDebugCBuffer->Bind();
		mDebugData->mDebugBuffer.Allocate(mDebugData->mDebugCBuffer->GetSize());
		mDebugData->mDebugBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the grid rendering
		mDebugData->mGridCBuffer = ConstantBufferLibrary::Load("Grid", 144, D3D11_PIXEL_SHADER, 10);
		mDebugData->mGridCBuffer->Bind();
		mDebugData->mGridBuffer.Allocate(mDebugData->mGridCBuffer->GetSize());
		mDebugData->mGridBuffer.ZeroInitialize();
	}

	void RendererDebug::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

		delete[] mDebugData->LineVertexBufferBase;
	}

	void RendererDebug::BeginScene(const EditorCamera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		ZeroMemory(mDebugData->LineVertexBufferBase, mDebugData->MaxVertices * sizeof(LineVertex));
		mDebugData->LineVertexCount = 0;

		// Updating the camera data in the buffer and mapping it to the GPU
		mDebugData->mDebugBuffer.Write((void*)&camera.GetViewMatrix(), 64, 0);
		mDebugData->mDebugBuffer.Write((void*)&camera.GetProjection(), 64, 64);
		mDebugData->mDebugBuffer.Write((void*)&DirectX::XMMatrixInverse(nullptr, camera.GetViewMatrix()), 64, 128);
		mDebugData->mDebugBuffer.Write((void*)&DirectX::XMMatrixInverse(nullptr, camera.GetProjection()), 64, 192);
		mDebugData->mDebugBuffer.Write((void*)&camera.GetPosition(), 16, 256);
		mDebugData->mDebugCBuffer->Map(mDebugData->mDebugBuffer);

		mDebugData->LineVertexBuffer->Bind();

		mDebugData->LineVertexBufferPtr = mDebugData->LineVertexBufferBase;
	}

	void RendererDebug::EndScene(const bool debugActivated)
	{
		TOAST_PROFILE_FUNCTION();

		DebugPass();

		if (debugActivated)
		{
			RenderCommand::BindBackbuffer();
			RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
		}
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
		mDebugData->LineVertexBufferPtr->Position = p1;
		mDebugData->LineVertexBufferPtr->Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		mDebugData->LineVertexBufferPtr++;
		
		mDebugData->LineVertexBufferPtr->Position = p2;
		mDebugData->LineVertexBufferPtr->Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		mDebugData->LineVertexBufferPtr++;

		mDebugData->LineVertexCount += 2;
	}

	void RendererDebug::SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2)
	{
		DirectX::XMFLOAT3 p1f;
		DirectX::XMStoreFloat3(&p1f, p1);
		DirectX::XMFLOAT3 p2f;
		DirectX::XMStoreFloat3(&p2f, p2);

		RendererDebug::SubmitLine(p1f, p2f);
	}

	void RendererDebug::SubmitGrid(const EditorCamera& camera)
	{
		mDebugData->GridData.ViewMatrix = camera.GetViewMatrix();
		mDebugData->GridData.ProjectionMatrix = camera.GetProjection();
		mDebugData->GridData.FarClip = camera.GetFarClip();
		mDebugData->GridData.NearClip = camera.GetNearClip();
	}

	void RendererDebug::DebugPass()
	{
		sRendererData->BaseFramebuffer->Bind();
		mDebugData->DebugShader->Bind();
		RenderCommand::EnableBlending();

		// Frustum
		RenderCommand::SetPrimitiveTopology(Topology::LINELIST);

		if (mDebugData->LineVertexCount != 0)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)mDebugData->LineVertexBufferPtr - (uint8_t*)mDebugData->LineVertexBufferBase);
			mDebugData->LineVertexBuffer->SetData(mDebugData->LineVertexBufferBase, dataSize);

			RenderCommand::Draw(mDebugData->LineVertexCount);
		}

		//Grid
		RenderCommand::DisableWireframe();
		RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

		mDebugData->mGridBuffer.Write((void*)&mDebugData->GridData.ViewMatrix, 64, 0);
		mDebugData->mGridBuffer.Write((void*)&mDebugData->GridData.ProjectionMatrix, 64, 64);
		mDebugData->mGridBuffer.Write((void*)&mDebugData->GridData.FarClip, 4, 128);
		mDebugData->mGridBuffer.Write((void*)&mDebugData->GridData.NearClip, 4, 132);
		mDebugData->mGridCBuffer->Map(mDebugData->mGridBuffer);
		mDebugData->GridShader->Bind();

		RenderCommand::Draw(6);
	}

}