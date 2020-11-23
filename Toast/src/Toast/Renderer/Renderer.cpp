#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

namespace Toast {

	struct RendererData
	{
		Renderer::Statistics Stats;
	};

	static RendererData sData;

	Scope<Renderer::SceneData> Renderer::mSceneData = CreateScope<Renderer::SceneData>();

	void Renderer::Init()
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();
		RendererDebug::Init();
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
		RendererDebug::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::ResizeViewport(0, 0, width, height);
	}

	void Renderer::BeginScene(const Camera& camera, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMFLOAT4 cameraPos)
	{
		TOAST_PROFILE_FUNCTION();

		mSceneData->viewMatrix = viewMatrix;
		mSceneData->inverseViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		mSceneData->projectionMatrix = camera.GetProjection();
		mSceneData->cameraPos = cameraPos;
		MaterialLibrary::Get("Standard")->SetData("Camera", (void*)&mSceneData->viewMatrix);
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<BufferLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform)
	{
		bufferLayout->Bind();
		vertexBuffer->Bind();
		indexBuffer->Bind();
		shader->Bind();

		//RenderCommand::DrawIndexed(indexBuffer);
	}

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe)
	{
		if (mesh->mVertexBuffer) mesh->mVertexBuffer->Bind();
		if (mesh->mIndexBuffer)	mesh->mIndexBuffer->Bind();

		if (wireframe)
			RenderCommand::EnableWireframeRendering();
		else
			RenderCommand::DisableWireframeRendering();

		mesh->mMaterial->SetData("Model", (void*)&transform);
		mesh->mMaterial->Bind();

		RenderCommand::SetPrimitiveTopology(mesh->mTopology);

		for (Submesh& submesh : mesh->mSubmeshes)
			RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
	}

	void Renderer::SubmitPlanet(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, PlanetComponent::PlanetGPUData planetData, PlanetComponent::MorphGPUData morphData, bool wireframe)
	{
		if(mesh->mVertexBuffer)	mesh->mVertexBuffer->Bind();
		if (mesh->mInstanceVertexBuffer) mesh->mInstanceVertexBuffer->Bind();
		if (mesh->mIndexBuffer)	mesh->mIndexBuffer->Bind();

		if (wireframe)
			RenderCommand::EnableWireframeRendering();
		else
			RenderCommand::DisableWireframeRendering();

		mesh->mMaterial->SetData("Morphing", (void*)&morphData);
		mesh->mMaterial->SetData("Planet", (void*)&planetData);
		mesh->mMaterial->SetData("Model", (void*)&transform);
		mesh->mMaterial->Bind();

		RenderCommand::SetPrimitiveTopology(mesh->mTopology);

		for (Submesh& submesh : mesh->mSubmeshes)
		{
			if (mesh->mPlanetPatches.size() > 495000)
				TOAST_CORE_WARN("Number of instances getting to high: {0}", mesh->mPlanetPatches.size());

			RenderCommand::DrawIndexedInstanced(submesh.IndexCount, mesh->mPlanetPatches.size(), 0, 0, 0);
		}
	}

	void Renderer::ResetStats()
	{
		memset(&sData.Stats, 0, sizeof(Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return sData.Stats;
	}
}