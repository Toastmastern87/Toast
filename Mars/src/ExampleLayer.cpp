#include "ExampleLayer.h"

#include <imgui/imgui.h>


ExampleLayer::ExampleLayer()
	: Layer("Example"), mCameraController(1280.0f / 720.0f, true)
{
	float vertices[5 * 4] = {
							-0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
							0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
							0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
							-0.5f,  0.5f, 0.0f, 0.0f, 0.0f
	};

	mVertexBuffer = Toast::VertexBuffer::Create(vertices, sizeof(vertices), uint32_t(4));

	uint32_t indices[6] = { 0, 2, 1, 2, 0, 3 };

	mIndexBuffer = Toast::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t));

	auto shader = mShaderLibrary.Load("assets/shaders/Test.hlsl");

	const std::initializer_list<Toast::BufferLayout::BufferElement>& layout = {
																{ Toast::ShaderDataType::Float3, "POSITION" },
																{ Toast::ShaderDataType::Float2, "TEXCOORD" },
	};

	mBufferLayout = Toast::BufferLayout::Create(layout, shader);

	auto textureShader = mShaderLibrary.Load("assets/shaders/Texture.hlsl");

	mTextureBufferLayout = Toast::BufferLayout::Create(layout, textureShader);

	mTexture = Toast::Texture2D::Create("assets/textures/Checkerboard.png", 1);

	mMarsLogoTexture = Toast::Texture2D::Create("assets/textures/Logo.png", 2);
}

void ExampleLayer::OnAttach() 
{
}

void ExampleLayer::OnDetach()
{
}

void ExampleLayer::OnUpdate(Toast::Timestep ts)
{
	// Update
	mCameraController.OnUpdate(ts);

	// Render
	const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	Toast::RenderCommand::SetRenderTargets();
	Toast::RenderCommand::Clear(clearColor);

	Toast::Renderer::BeginScene(mCameraController.GetCamera());

	DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f);

	auto shader = mShaderLibrary.Get("Test");

	shader->SetColorData(DirectX::XMFLOAT4(mSquareColor[0], mSquareColor[1], mSquareColor[2], 1.0f), 1.0f);

	for (int y = 0; y < 10; y++)
	{
		for (int x = 0; x < 10; x++)
		{
			DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * scale;
			transform = transform * DirectX::XMMatrixTranslation(x * 0.11f, y * 0.11f, 0.0f);

			Toast::Renderer::Submit(mIndexBuffer, shader, mBufferLayout, mVertexBuffer, transform);
		}
	}

	auto textureShader = mShaderLibrary.Get("Texture");

	mTexture->Bind();
	Toast::Renderer::Submit(mIndexBuffer, textureShader, mTextureBufferLayout, mVertexBuffer, DirectX::XMMatrixScaling(1.5f, 1.5f, 1.5f));

	mMarsLogoTexture->Bind();
	Toast::Renderer::Submit(mIndexBuffer, textureShader, mTextureBufferLayout, mVertexBuffer, DirectX::XMMatrixScaling(1.5f, 1.5f, 1.5f));

	Toast::Renderer::EndScene();
}

void ExampleLayer::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit3("Square Color", mSquareColor);
	ImGui::End();
}

void ExampleLayer::OnEvent(Toast::Event& e)
{
	mCameraController.OnEvent(e);
}