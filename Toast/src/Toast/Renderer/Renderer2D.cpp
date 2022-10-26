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
				//Transform to correct size and position
				drawCommand.Element->Transform(drawCommand.Transform);
				drawCommand.Element->SetWidth(100.0f);
				drawCommand.Element->SetHeight(100.0f);

				Ref<UIPanel> panel = std::dynamic_pointer_cast<UIPanel>(drawCommand.Element);
				panel->Bind();

				RenderCommand::DrawIndexed(0, 0, 6);
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

	void Renderer2D::SubmitQuad(const DirectX::XMMATRIX& transform, const Ref<UIPanel>& panel)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(panel, transform, ElementType::Panel);
	}

}