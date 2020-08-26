#include "TheNextFrontier2D.h"

#include <imgui/imgui.h>

TheNextFrontier2D::TheNextFrontier2D()
	: Layer("TheNextFrontier2D"), mCameraController(1280.0f / 720.0f, true)
{
}

void TheNextFrontier2D::OnAttach()
{
	TOAST_PROFILE_FUNCTION();

	mCheckerboardTexture = Toast::Texture2D::Create("assets/textures/Checkerboard.png", 1);
}

void TheNextFrontier2D::OnDetach()
{
	TOAST_PROFILE_FUNCTION();
}

void TheNextFrontier2D::OnUpdate(Toast::Timestep ts)
{
	TOAST_PROFILE_FUNCTION();

	// Update
	{
		TOAST_PROFILE_SCOPE("CameraController::OnUpdate");
		mCameraController.OnUpdate(ts);
	}

	// Render
	Toast::Renderer2D::ResetStats();
	{
		TOAST_PROFILE_SCOPE("Renderer Prep");
		const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

		Toast::RenderCommand::SetRenderTargets();
		Toast::RenderCommand::Clear(clearColor);
	}

	{
		static float rotation = 0.0f;
		rotation += (ts * 50.0f);
		
		TOAST_PROFILE_SCOPE("Renderer Draw");
		Toast::Renderer2D::BeginScene(mCameraController.GetCamera());
		Toast::Renderer2D::DrawRotatedQuad(DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.8f, 0.8f), DirectX::XMConvertToRadians(-45.0f), DirectX::XMFLOAT4(0.8f, 0.2f, 0.3f, 1.0f));
		Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT2(0.5f, -0.5f), DirectX::XMFLOAT2(0.5f, 0.75f), DirectX::XMFLOAT4(0.8f, 0.2f, 0.3f, 1.0f));
		Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT2(-1.0f, 0.0f), DirectX::XMFLOAT2(0.8f, 0.8f), DirectX::XMFLOAT4(mSquareColor[0], mSquareColor[1], mSquareColor[2], mSquareColor[3]));
		Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT3(0.0f, 0.0f, 0.1f), DirectX::XMFLOAT2(20.0f, 20.0f), mCheckerboardTexture, 10.0f);
		Toast::Renderer2D::DrawRotatedQuad(DirectX::XMFLOAT3(-2.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMConvertToRadians(rotation), mCheckerboardTexture, 20.0f);
		Toast::Renderer2D::EndScene();

		Toast::Renderer2D::BeginScene(mCameraController.GetCamera());
		for (float y = -5.0f; y < 5.0f; y += 0.5f) 
		{
			for (float x = -5.0f; x < 5.0f; x += 0.5f)
			{
				DirectX::XMFLOAT4 color = DirectX::XMFLOAT4(((x + 5.0f) / 10.0f), 0.4f, ((y + 5.0f) / 10.0f), 0.7f);
				Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT2(x, y), DirectX::XMFLOAT2(0.45f, 0.45f), color);
			}
		}
		Toast::Renderer2D::EndScene();
	}
}

void TheNextFrontier2D::OnImGuiRender()
{
	TOAST_PROFILE_FUNCTION();

	ImGui::Begin("Settings");

	auto stats = Toast::Renderer2D::GetStats();
	ImGui::Text("Renderer2D Stats: ");
	ImGui::Text("Draw Calls: %d", stats.DrawCalls);
	ImGui::Text("Quads: %d", stats.QuadCount);
	ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
	ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

	ImGui::ColorEdit4("Square Color", mSquareColor);
	ImGui::End();
}

void TheNextFrontier2D::OnEvent(Toast::Event& e)
{
	mCameraController.OnEvent(e);
}