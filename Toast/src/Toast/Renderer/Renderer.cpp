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
		// TODO Get the far and near values from the actual camera instead of hardcoded, gradient should come from somekind of option panel in ImGui
		mSceneData->farClip = 10000.0f;
		mSceneData->nearClip = 0.01f;
		mSceneData->gradient = 150.0f;
		mSceneData->inverseViewMatrix = DirectX::XMMatrixInverse(nullptr, DirectX::XMMatrixInverse(nullptr, transform));
		mSceneData->inverseProjectionMatrix = DirectX::XMMatrixInverse(nullptr, camera.GetProjection());
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<BufferLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform)
	{
		bufferLayout->Bind();
		vertexBuffer->Bind();
		indexBuffer->Bind();
		shader->SetData("Camera", (void*)&mSceneData->inverseViewMatrix);
		shader->Bind();

		RenderCommand::DrawIndexed(indexBuffer);
	}

	void Renderer::SubmitQuad(const DirectX::XMMATRIX& transform)
	{
		TOAST_PROFILE_FUNCTION();

		sData.GridShader->SetData("GridData", (void*)&mSceneData->viewMatrix);
		sData.GridShader->SetData("InverseMatrices", (void*)&mSceneData->inverseViewMatrix);
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