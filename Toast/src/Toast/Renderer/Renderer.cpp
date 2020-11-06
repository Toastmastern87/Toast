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

	void Renderer::BeginScene(const Camera& camera, const DirectX::XMMATRIX& viewMatrix)
	{
		TOAST_PROFILE_FUNCTION();

		mSceneData->viewMatrix = viewMatrix;
		mSceneData->projectionMatrix = camera.GetProjection();
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

		RenderCommand::DrawIndexed(indexBuffer);
	}

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe)
	{
		if (wireframe)
			RenderCommand::EnableWireframeRendering();
		else
			RenderCommand::DisableWireframeRendering();

		mesh->GetMeshShader()->SetData("Camera", (void*)&mSceneData->viewMatrix);
		mesh->GetMeshShader()->SetData("Model", (void*)&transform);
		mesh->GetLayout()->Bind();
		mesh->GetVertexBuffer()->Bind();
		mesh->GetIndexBuffer()->Bind();
		mesh->GetMeshShader()->Bind();
		
		RenderCommand::DrawIndexed(mesh->GetIndexBuffer(), mesh->GetIndexBuffer()->GetCount());
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