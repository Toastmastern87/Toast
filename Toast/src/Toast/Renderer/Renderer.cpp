#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"

namespace Toast {

	struct RendererData
	{
		Ref<BufferLayout> GridBufferLayout;
		Ref<Shader> GridShader;

		Renderer::Statistics Stats;
	};

	static RendererData sData;

	Scope<Renderer::SceneData> Renderer::mSceneData = CreateScope<Renderer::SceneData>();

	void Renderer::Init()
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();

		// TODO - Should be moved to the editor instead???
		sData.GridShader = Shader::Create("assets/shaders/Grid.hlsl");

		const std::initializer_list<BufferLayout::BufferElement>& layout = {};

		sData.GridBufferLayout = BufferLayout::Create(layout, sData.GridShader);
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::ResizeViewport(0, 0, width, height);
	}

	void Renderer::BeginScene(const Camera& camera, const DirectX::XMMATRIX& transform)
	{
		TOAST_PROFILE_FUNCTION();

		mSceneData->viewMatrix = DirectX::XMMatrixInverse(nullptr, transform);
		mSceneData->projectionMatrix = camera.GetProjection();
		mSceneData->inverseViewMatrix = DirectX::XMMatrixInverse(nullptr, DirectX::XMMatrixInverse(nullptr, transform));
		mSceneData->inverseProjectionMatrix = DirectX::XMMatrixInverse(nullptr, camera.GetProjection());

		sData.GridShader->SetData("Camera", (void*)&mSceneData->viewMatrix);
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

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform)
	{
		mesh->GetMeshShader()->SetData("Camera", (void*)&mSceneData->viewMatrix);
		mesh->GetMeshShader()->SetData("Model", (void*)&transform);
		mesh->GetLayout()->Bind();
		mesh->GetVertexBuffer()->Bind();
		mesh->GetIndexBuffer()->Bind();
		mesh->GetMeshShader()->Bind();
		
		RenderCommand::DrawIndexed(mesh->GetIndexBuffer(), mesh->GetIndexBuffer()->GetCount());
	}

	void Renderer::SubmitQuad(const DirectX::XMMATRIX& transform)
	{
	}

	// TODO - When material system is implemented this should be handled by the SubmitMesh()	
	void Renderer::SubmitGrid(const Camera& camera, const DirectX::XMMATRIX& transform, const DirectX::XMFLOAT3 gridData)
	{
		TOAST_PROFILE_FUNCTION();

		struct Data 
		{
			DirectX::XMMATRIX viewMatrix, projectionMatrix;
			DirectX::XMFLOAT3 floats;
		}; 

		Data data;
		data.viewMatrix = DirectX::XMMatrixInverse(nullptr, transform);
		data.projectionMatrix = camera.GetProjection();
		data.floats = gridData;

		sData.GridShader->SetData("GridData", (void*)&data);
		sData.GridBufferLayout->Bind();
		sData.GridShader->Bind();

		RenderCommand::Draw(6);
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