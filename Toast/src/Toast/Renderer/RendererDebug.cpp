#include "tpch.h"
#include "RendererDebug.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"

using namespace DirectX;

namespace Toast {

	Scope<RendererDebug::DebugData> RendererDebug::mDebugData = CreateScope<RendererDebug::DebugData>();

	void RendererDebug::Init(uint32_t width, uint32_t height)
	{
		TOAST_PROFILE_FUNCTION();

		mDebugData->LineVertexBuffer = CreateRef<VertexBuffer>(mDebugData->MaxVertices * sizeof(Vertex), mDebugData->MaxVertices, 0);

		mDebugData->LineVertexBufferBase = new Vertex[mDebugData->MaxVertices];

		mDebugData->DebugShader = CreateRef<Shader>("assets/shaders/Debug/Debug.hlsl");
		mDebugData->GridShader = CreateRef<Shader>("assets/shaders/Debug/Grid.hlsl");
		mDebugData->ObjectMaskShader = CreateRef<Shader>("assets/shaders/Debug/ObjectMask.hlsl");
		mDebugData->OutlineShader = CreateRef<Shader>("assets/shaders/Debug/Outline.hlsl");

		// Setting up the constant buffer and data buffer for the debug rendering data
		mDebugData->mDebugCBuffer = ConstantBufferLibrary::Load("Camera", 288, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::Camera), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Camera) });
		mDebugData->mDebugCBuffer->Bind();
		mDebugData->mDebugBuffer.Allocate(mDebugData->mDebugCBuffer->GetSize());
		mDebugData->mDebugBuffer.ZeroInitialize();

		// Setting up the Render Target for 
		mDebugData->SelectedMeshMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
	}

	void RendererDebug::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

		delete[] mDebugData->LineVertexBufferBase;
	}

	void RendererDebug::OnWindowResize(uint32_t width, uint32_t height)
	{
		mDebugData->SelectedMeshMaskRT->Resize(width, height);
	}

	void RendererDebug::BeginScene(Camera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		// Updating the camera data in the buffer and mapping it to the GPU
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 0);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetProjection(), 64, 64);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetInvViewMatrix(), 64, 128);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetInvProjection(), 64, 192);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetFarClip(), 4, 272);
		mDebugData->mDebugBuffer.Write((uint8_t*)&camera.GetNearClip(), 4, 276);
		mDebugData->mDebugCBuffer->Map(mDebugData->mDebugBuffer);

		mDebugData->LineVertexBufferPtr = mDebugData->LineVertexBufferBase;
	}

	void RendererDebug::EndScene(const bool debugActivated, const bool runtime, const bool renderUI, const bool renderGrid)
	{
		TOAST_PROFILE_FUNCTION();

		if (!runtime)
			OutlineRenderPass();
		DebugRenderPass(runtime, renderGrid);

		if (debugActivated && !renderUI)
		{
			RenderCommand::SetRenderTargets({ sRendererData->BackbufferRT->GetRTV().Get() }, nullptr);
			RenderCommand::ClearRenderTargets(sRendererData->BackbufferRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });
		}

		ZeroMemory(mDebugData->LineVertexBufferBase, mDebugData->MaxVertices * sizeof(Vertex));
		mDebugData->LineVertexCount = 0;

		if (!runtime) 
			sRendererData->MeshSelectedDrawList.clear();
		
		sRendererData->MeshColliderDrawList.clear();

		//TOAST_CORE_CRITICAL("DEBUG END SCENE!");
	}

	void RendererDebug::SubmitCameraFrustum(Ref<Frustum> frustum)
	{
		RendererDebug::SubmitLine(frustum->mNearTopLeft, frustum->mFarTopLeft, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearTopRight, frustum->mFarTopRight, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomLeft, frustum->mFarBottomLeft, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomRight, frustum->mFarBottomRight, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));

		RendererDebug::SubmitLine(frustum->mNearTopLeft, frustum->mNearTopRight, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomRight, frustum->mNearBottomLeft, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomLeft, frustum->mNearTopLeft, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mNearBottomRight, frustum->mNearTopRight, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));

		RendererDebug::SubmitLine(frustum->mFarTopLeft, frustum->mFarTopRight, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mFarBottomRight, frustum->mFarBottomLeft, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mFarBottomLeft, frustum->mFarTopLeft, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
		RendererDebug::SubmitLine(frustum->mFarBottomRight, frustum->mFarTopRight, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
	}

	void RendererDebug::SubmitLine(DirectX::XMFLOAT3& p1, DirectX::XMFLOAT3& p2, DirectX::XMFLOAT3& color)
	{
		mDebugData->LineVertexBufferPtr->Position = p1;
		mDebugData->LineVertexBufferPtr->Color = color;
		mDebugData->LineVertexBufferPtr->Normal = { 0.0f, 0.0f, 0.0f };
		mDebugData->LineVertexBufferPtr->Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
		mDebugData->LineVertexBufferPtr->Texcoord = { 0.0f, 0.0f };
		mDebugData->LineVertexBufferPtr++;
		
		mDebugData->LineVertexBufferPtr->Position = p2;
		mDebugData->LineVertexBufferPtr->Color = color;
		mDebugData->LineVertexBufferPtr->Normal = { 0.0f, 0.0f, 0.0f };
		mDebugData->LineVertexBufferPtr->Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
		mDebugData->LineVertexBufferPtr->Texcoord = { 0.0f, 0.0f };
		mDebugData->LineVertexBufferPtr++;

		mDebugData->LineVertexCount += 2;

		//TOAST_CORE_INFO("Adding line! Count number: %d", mDebugData->LineVertexCount);
	}

	void RendererDebug::SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2, DirectX::XMFLOAT3& color)
	{
		DirectX::XMFLOAT3 p1f;
		DirectX::XMStoreFloat3(&p1f, p1);
		DirectX::XMFLOAT3 p2f;
		DirectX::XMStoreFloat3(&p2f, p2);

		RendererDebug::SubmitLine(p1f, p2f, color);
	}

	void RendererDebug::SubmitLine(Vector3& p1, Vector3& p2, DirectX::XMFLOAT3& color)
	{
		DirectX::XMFLOAT3 p1f = { (float)p1.x, (float)p1.y, (float)p1.z };
		DirectX::XMFLOAT3 p2f = { (float)p2.x, (float)p2.y, (float)p2.z };

		RendererDebug::SubmitLine(p1f, p2f, color);
	}

	void RendererDebug::SubmitCollider(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe)
	{
		sRendererData->MeshColliderDrawList.emplace_back(mesh, transform, wireframe);
	}

	void RendererDebug::DebugRenderPass(const bool runtime, const bool renderGrid)
	{
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Debug Render Pass");
#endif
		int noWorldTransform;

		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);

		mDebugData->DebugShader->Bind();

		mDebugData->LineVertexBuffer->Bind();

		if (mDebugData->LineVertexCount != 0)
		{
			// Frustum
			RenderCommand::SetPrimitiveTopology(Topology::LINELIST);

			uint32_t dataSize = (uint32_t)((uint8_t*)mDebugData->LineVertexBufferPtr - (uint8_t*)mDebugData->LineVertexBufferBase);
			mDebugData->LineVertexBuffer->SetData(mDebugData->LineVertexBufferBase, dataSize);

			noWorldTransform = 1;

			sRendererData->ModelBuffer.Write((uint8_t*)&noWorldTransform, 4, 68);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

			RenderCommand::Draw(mDebugData->LineVertexCount);
		}

		RenderCommand::SetRasterizerState(sRendererData->WireframeRasterizerState);
		RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

		for (const auto& meshCommand : sRendererData->MeshColliderDrawList)
		{
			for (Submesh& submesh : meshCommand.Mesh->mLODGroups[0]->Submeshes)
			{
				noWorldTransform = 0;

				sRendererData->ModelBuffer.Write((uint8_t*)&DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform), 64, 0);
				sRendererData->ModelBuffer.Write((uint8_t*)&noWorldTransform, 4, 68);
				sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

				meshCommand.Mesh->Bind();

				RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
			}
		}	

		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);

		//Grid
		if (!runtime && renderGrid)
		{
			RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

			mDebugData->GridShader->Bind();

			RenderCommand::SetBlendState(sRendererData->AtmospherePassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

			RenderCommand::Draw(6);
		}

		RenderCommand::ClearShaderResources();

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

		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetRenderTargets({ mDebugData->SelectedMeshMaskRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::ClearRenderTargets(mDebugData->SelectedMeshMaskRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });

		mDebugData->ObjectMaskShader->Bind();

		// TODO: This should be done in a single draw call, will be fixed with the updated star ship model.
		// Mask out the selected meshes
		for (const auto& meshCommand : sRendererData->MeshSelectedDrawList)
		{
			meshCommand.Mesh->Bind();

			int isInstanced = meshCommand.Mesh->IsInstanced() ? 1 : 0;

			// Model data
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 64);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 68);
			sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 72);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

			RenderCommand::DrawIndexed(0, 0, meshCommand.Mesh->GetIndices().size());
		}

		// Draw the outline
		mDebugData->OutlineShader->Bind();
		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 11, mDebugData->SelectedMeshMaskRT->GetSRV());

		Renderer::DrawFullscreenQuad();

		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

}