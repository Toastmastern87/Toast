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
		mDebugData->SelectedMeshMaskShader = CreateRef<Shader>("assets/shaders/SelectedMeshMask.hlsl");
		mDebugData->OutlineShader = CreateRef<Shader>("assets/shaders/Outline.hlsl");

		// Setting up the constant buffer and data buffer for the debug rendering data
		mDebugData->mDebugCBuffer = ConstantBufferLibrary::Load("Camera", 304, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 0), CBufferBindInfo(D3D11_PIXEL_SHADER, 11) });
		mDebugData->mDebugCBuffer->Bind();
		mDebugData->mDebugBuffer.Allocate(mDebugData->mDebugCBuffer->GetSize());
		mDebugData->mDebugBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the grid rendering
		mDebugData->mGridCBuffer = ConstantBufferLibrary::Load("Grid", 144, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, 10) });
		mDebugData->mGridCBuffer->Bind();
		mDebugData->mGridBuffer.Allocate(mDebugData->mGridCBuffer->GetSize());
		mDebugData->mGridBuffer.ZeroInitialize();
	}

	void RendererDebug::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

		delete[] mDebugData->LineVertexBufferBase;
	}

	void RendererDebug::BeginScene(Camera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		// Updating the camera data in the buffer and mapping it to the GPU
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 0);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetProjection(), 64, 64);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetInvViewMatrix(), 64, 128);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetInvProjection(), 64, 192);
		//mDebugData->mDebugBuffer.Write((void*)&camera.GetPosition(), 16, 256);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetForwardDirection(), 16, 272);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetFarClip(), 4, 288);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetNearClip(), 4, 292);
		mDebugData->mDebugCBuffer->Map(mDebugData->mDebugBuffer);

		mDebugData->LineVertexBuffer->Bind();

		mDebugData->LineVertexBufferPtr = mDebugData->LineVertexBufferBase;
	}

	void RendererDebug::EndScene(const bool debugActivated, const bool runtime, const bool renderUI)
	{
		TOAST_PROFILE_FUNCTION();

		if(!runtime)
			OutlineRenderPass();
		DebugRenderPass(runtime);

		if (debugActivated && !renderUI)
		{
			RenderCommand::BindBackbuffer();
			RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
		}

		ZeroMemory(mDebugData->LineVertexBufferBase, mDebugData->MaxVertices * sizeof(LineVertex));
		mDebugData->LineVertexCount = 0;

		if (!runtime) 
		{
			mDebugData->mGridBuffer.ZeroInitialize();
			mDebugData->mGridCBuffer->Map(mDebugData->mGridBuffer);

			sRendererData->MeshSelectedDrawList.clear();
		}
		sRendererData->MeshColliderDrawList.clear();

		//TOAST_CORE_CRITICAL("DEBUG END SCENE!");
	}

	void RendererDebug::SubmitCameraFrustum(Ref<Frustum> frustum)
	{
		RendererDebug::SubmitLine(frustum->mNearTopLeft, frustum->mFarTopLeft, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearTopRight, frustum->mFarTopRight, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomLeft, frustum->mFarBottomLeft, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomRight, frustum->mFarBottomRight, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

		RendererDebug::SubmitLine(frustum->mNearTopLeft, frustum->mNearTopRight, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomRight, frustum->mNearBottomLeft, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomLeft, frustum->mNearTopLeft, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomRight, frustum->mNearTopRight, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

		RendererDebug::SubmitLine(frustum->mFarTopLeft, frustum->mFarTopRight, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mFarBottomRight, frustum->mFarBottomLeft, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mFarBottomLeft, frustum->mFarTopLeft, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mFarBottomRight, frustum->mFarTopRight, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	void RendererDebug::SubmitLine(DirectX::XMFLOAT3& p1, DirectX::XMFLOAT3& p2, DirectX::XMFLOAT4& color)
	{
		mDebugData->LineVertexBufferPtr->Position = p1;
		mDebugData->LineVertexBufferPtr->Color = color;
		mDebugData->LineVertexBufferPtr++;
		
		mDebugData->LineVertexBufferPtr->Position = p2;
		mDebugData->LineVertexBufferPtr->Color = color;
		mDebugData->LineVertexBufferPtr++;

		mDebugData->LineVertexCount += 2;
	}

	void RendererDebug::SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2, DirectX::XMFLOAT4& color)
	{
		DirectX::XMFLOAT3 p1f;
		DirectX::XMStoreFloat3(&p1f, p1);
		DirectX::XMFLOAT3 p2f;
		DirectX::XMStoreFloat3(&p2f, p2);

		RendererDebug::SubmitLine(p1f, p2f, color);
	}

	void RendererDebug::SubmitLine(Vector3& p1, Vector3& p2, DirectX::XMFLOAT4& color)
	{
		DirectX::XMFLOAT3 p1f = { (float)p1.x, (float)p1.y, (float)p1.z };
		DirectX::XMFLOAT3 p2f = { (float)p2.x, (float)p2.y, (float)p2.z };

		RendererDebug::SubmitLine(p1f, p2f, color);
	}

	void RendererDebug::SubmitGrid(
		EditorCamera& camera)
	{
		mDebugData->mGridBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 0);
		mDebugData->mGridBuffer.Write((uint8_t*)&camera.GetProjection(), 64, 64);
		mDebugData->mGridBuffer.Write((uint8_t*)&camera.GetFarClip(), 4, 128);
		mDebugData->mGridBuffer.Write((uint8_t*)&camera.GetNearClip(), 4, 132);
	}

	void RendererDebug::SubmitCollider(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe)
	{
			sRendererData->MeshColliderDrawList.emplace_back(mesh, transform, wireframe);
	}

	void RendererDebug::DebugRenderPass(const bool runtime)
	{
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Debug Render Pass");
#endif

		sRendererData->FinalFramebuffer->EnableDepth();
		sRendererData->FinalFramebuffer->Bind();

		RenderCommand::EnableBlending();

		//if (!runtime)
		//{
			if (mDebugData->LineVertexCount != 0)
			{
				mDebugData->DebugShader->Bind();

				// Frustum
				RenderCommand::SetPrimitiveTopology(Topology::LINELIST);

				uint32_t dataSize = (uint32_t)((uint8_t*)mDebugData->LineVertexBufferPtr - (uint8_t*)mDebugData->LineVertexBufferBase);
				mDebugData->LineVertexBuffer->SetData(mDebugData->LineVertexBufferBase, dataSize);

				RenderCommand::Draw(mDebugData->LineVertexCount);
			}

			//Grid
			RenderCommand::DisableWireframe();
			RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

			mDebugData->mGridCBuffer->Map(mDebugData->mGridBuffer);
			mDebugData->GridShader->Bind();

			RenderCommand::Draw(6);
		//}

		RenderCommand::EnableWireframe();

		for (const auto& meshCommand : sRendererData->MeshColliderDrawList)
		{
			for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
			{
				meshCommand.Mesh->Set<DirectX::XMMATRIX>("Standard", "Model", "worldMatrix", DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform));

				meshCommand.Mesh->Map("Standard");
				meshCommand.Mesh->Bind("Standard", true);

				RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
			}
		}	

		RenderCommand::DisableWireframe();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void RendererDebug::OutlineRenderPass()
	{
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Outline Render Pass");
#endif

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		sRendererData->OutlineFramebuffer->Bind();
		sRendererData->OutlineFramebuffer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });

		for (const auto& meshCommand : sRendererData->MeshSelectedDrawList) 
		{
			if (meshCommand.Mesh->mVertexBuffer)	meshCommand.Mesh->mVertexBuffer->Bind();
			//if (meshCommand.Mesh->mInstanceVertexBuffer && meshCommand.PlanetData) meshCommand.Mesh->mInstanceVertexBuffer->Bind();
			if (meshCommand.Mesh->mIndexBuffer)		meshCommand.Mesh->mIndexBuffer->Bind();

			if (meshCommand.Wireframe)
				RenderCommand::EnableWireframe();
			else
				RenderCommand::DisableWireframe();

			RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);

			for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
			{
				meshCommand.Mesh->Set<DirectX::XMMATRIX>(submesh.MaterialName, "Model", "worldMatrix", DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform));
				meshCommand.Mesh->Map(submesh.MaterialName);
				meshCommand.Mesh->Bind(submesh.MaterialName);

				mDebugData->SelectedMeshMaskShader->Bind();

				RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);

				ID3D11RenderTargetView* nullRTV = nullptr;
				deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
				
				sRendererData->FinalFramebuffer->Bind();

				auto temp = sRendererData->OutlineFramebuffer->GetSRV(0);
				deviceContext->PSSetShaderResources(9, 1, temp.GetAddressOf());

				mDebugData->OutlineShader->Bind();

				RenderCommand::Draw(3);	

				ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
				deviceContext->PSSetShaderResources(9, 1, nullSRV);
			}
		}

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

}