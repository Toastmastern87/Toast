#include "tpch.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"

namespace Toast {

	Scope<Renderer2D::Renderer2DData> Renderer2D::sRenderer2DData = CreateScope<Renderer2D::Renderer2DData>();

	void Renderer2D::Init()
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->UIShader = CreateRef<Shader>("assets/shaders/UI.hlsl");

		// Setting up the constant buffer and data buffer for the camera rendering
		sRenderer2DData->UICBuffer = ConstantBufferLibrary::Load("UI", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 12), CBufferBindInfo(D3D11_PIXEL_SHADER, 12) });
		sRenderer2DData->UICBuffer->Bind();
		sRenderer2DData->UIBuffer.Allocate(sRenderer2DData->UICBuffer->GetSize());
		sRenderer2DData->UIBuffer.ZeroInitialize();
	}

	void Renderer2D::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void Renderer2D::BeginScene()
	{
		TOAST_PROFILE_FUNCTION();

		auto[width, height] = sRendererData->FinalRenderTarget->GetSize();

		float fWidth, fHeight;
		fWidth = (float)width;
		fHeight = (float)height;

		// Updating the UI data in the buffer and mapping it to the GPU
		sRenderer2DData->UIBuffer.Write((void*)&(float)fWidth, 4, 0);
		sRenderer2DData->UIBuffer.Write((void*)&(float)fHeight, 4, 4);
		sRenderer2DData->UICBuffer->Map(sRenderer2DData->UIBuffer);
	}

	void Renderer2D::EndScene()
	{
		TOAST_PROFILE_FUNCTION();

		sRendererData->FinalFramebuffer->DisableDepth();
		sRendererData->FinalFramebuffer->Bind();

		sRenderer2DData->UIShader->Bind();

		for (const auto& drawCommand : sRenderer2DData->ElementDrawList)
		{
			if (drawCommand.Type == ElementType::Panel) 
			{
				drawCommand.Element->SetTransform(drawCommand.Transform);

				DirectX::XMVECTOR vScale, vTransform, vRotation;
				DirectX::XMMatrixDecompose(&vScale, &vTransform, &vRotation, drawCommand.Transform);
				drawCommand.Element->SetWidth(DirectX::XMVectorGetX(vScale));
				drawCommand.Element->SetHeight(DirectX::XMVectorGetY(vScale));

				Ref<UIPanel> panel = std::dynamic_pointer_cast<UIPanel>(drawCommand.Element);
				panel->Bind();

				RenderCommand::DrawIndexed(0, 0, 6);
			}
			if (drawCommand.Type == ElementType::Text)
			{
				drawCommand.Element->SetTransform(drawCommand.Transform);

				Ref<UIText> text = std::dynamic_pointer_cast<UIText>(drawCommand.Element);
				text->Bind();

				RenderCommand::DrawIndexed(0, 0, text->GetQuadCount());
			}
		}

		ClearDrawList();
		
		RenderCommand::BindBackbuffer();
		RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

		sRendererData->FinalFramebuffer->EnableDepth();
	}

	void Renderer2D::ClearDrawList()
	{
		sRenderer2DData->ElementDrawList.clear();
	}

	void Renderer2D::SubmitPanel(const DirectX::XMMATRIX& transform, const Ref<UIPanel>& panel)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(panel, transform, ElementType::Panel);
	}

	void Renderer2D::SubmitText(const DirectX::XMMATRIX& transform, const Ref<UIText>& text)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(text, transform, ElementType::Text);
	}
}