#include "TheNextFrontier2D.h"

#include "imgui/imgui.h"

TheNextFrontier2D::TheNextFrontier2D()
	: Layer("TheNextFrontier2D"), mCameraController(1280.0f / 720.0f, true)
{
}

void TheNextFrontier2D::OnAttach()
{
	mCheckerboardTexture = Toast::Texture2D::Create("assets/textures/Checkerboard.png");
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

	Toast::Renderer2D::BeginScene(mCameraController.GetCamera());
	Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT2(-1.0f, 0.0f), DirectX::XMFLOAT2(0.8f, 0.8f), DirectX::XMFLOAT4(0.8f, 0.2f, 0.3f, 1.0f));
	Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT2(0.5f, -0.5f), DirectX::XMFLOAT2(0.5f, 0.75f), DirectX::XMFLOAT4(0.2f, 0.3f, 0.8f, 1.0f));
	Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT3(0.0f, 0.0f, 0.1f), DirectX::XMFLOAT2(10.0f, 10.0f), mCheckerboardTexture);
	Toast::Renderer2D::EndScene();
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