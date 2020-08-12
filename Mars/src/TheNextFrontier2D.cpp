#include "TheNextFrontier2D.h"

#include "Platform/DirectX/DirectXShader.h"

#include "imgui/imgui.h"

TheNextFrontier2D::TheNextFrontier2D()
	: Layer("TheNextFrontier2D"), mCameraController(1280.0f / 720.0f, true)
{

}

void TheNextFrontier2D::OnAttach()
{
	float vertices[3 * 4] = {
						-0.5f, -0.5f, 0.0f,
						0.5f, -0.5f, 0.0f,
						0.5f,  0.5f, 0.0f,
						-0.5f,  0.5f, 0.0f
	};

	mVertexBuffer.reset(Toast::VertexBuffer::Create(vertices, sizeof(vertices), uint32_t(4)));

	uint32_t indices[6] = { 0, 2, 1, 2, 0, 3 };

	mIndexBuffer.reset(Toast::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));

	mFlatColorShader = Toast::Shader::Create("assets/shaders/FlatColor.hlsl");

	const std::initializer_list<Toast::BufferLayout::BufferElement>& layout = {
															   { Toast::ShaderDataType::Float3, "POSITION" }
	};

	mBufferLayout.reset(Toast::BufferLayout::Create(layout, mFlatColorShader));
}

void TheNextFrontier2D::OnDetach()
{
}

void TheNextFrontier2D::OnUpdate(Toast::Timestep ts)
{
	// Update
	mCameraController.OnUpdate(ts);

	// Render
	const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	Toast::RenderCommand::SetRenderTargets();
	Toast::RenderCommand::Clear(clearColor);

	Toast::Renderer::BeginScene(mCameraController.GetCamera());

	std::static_pointer_cast<Toast::DirectXShader>(mFlatColorShader)->Bind();
	std::static_pointer_cast<Toast::DirectXShader>(mFlatColorShader)->UploadColorDataPSCBuffer(DirectX::XMFLOAT4(mSquareColor[0], mSquareColor[1], mSquareColor[2], mSquareColor[3]));

	Toast::Renderer::Submit(mIndexBuffer, mFlatColorShader, mBufferLayout, mVertexBuffer, DirectX::XMMatrixScaling(1.5f, 1.5f, 1.5f));

	Toast::Renderer::EndScene();
}

void TheNextFrontier2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", mSquareColor);
	ImGui::End();
}

void TheNextFrontier2D::OnEvent(Toast::Event& e)
{
	mCameraController.OnEvent(e);
}