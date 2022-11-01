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

		for (const auto& drawCommand : sRenderer2DData->ElementDrawList)
		{
			drawCommand.Element->Set<DirectX::XMMATRIX>("Model", "worldMatrix", drawCommand.Transform);
			drawCommand.Element->Set<int>("Model", "entityID", drawCommand.EntityID);

			drawCommand.Element->mShader->Bind();

			if (drawCommand.Type == ElementType::Panel) 
			{
				DirectX::XMVECTOR vScale, vTransform, vRotation;
				DirectX::XMMatrixDecompose(&vScale, &vTransform, &vRotation, drawCommand.Transform);
				drawCommand.Element->SetWidth(DirectX::XMVectorGetX(vScale));
				drawCommand.Element->SetHeight(DirectX::XMVectorGetY(vScale));

				Ref<UIPanel> panel = std::dynamic_pointer_cast<UIPanel>(drawCommand.Element);
				panel->Bind();

				RenderCommand::DrawIndexed(0, 0, 6);
			}
			if (drawCommand.Type == ElementType::Button)
			{
				DirectX::XMVECTOR vScale, vTransform, vRotation;
				DirectX::XMMatrixDecompose(&vScale, &vTransform, &vRotation, drawCommand.Transform);
				drawCommand.Element->SetWidth(DirectX::XMVectorGetX(vScale));
				drawCommand.Element->SetHeight(DirectX::XMVectorGetY(vScale));
				
				Ref<UIButton> button = std::dynamic_pointer_cast<UIButton>(drawCommand.Element);
				button->Bind();

				RenderCommand::DrawIndexed(0, 0, 6);
			}
			if (drawCommand.Type == ElementType::Text)
			{
				Ref<UIText> text = std::dynamic_pointer_cast<UIText>(drawCommand.Element);
				text->Bind();

				RenderCommand::DrawIndexed(0, 0, text->GetQuadCount());
			}
		}

		sRendererData->FinalFramebuffer->EnableDepth();

		// Picking
		sRendererData->PickingFramebuffer->Bind();
		RenderCommand::DisableBlending();

		for (const auto& drawCommand : sRenderer2DData->ElementDrawList)
		{
			drawCommand.Element->mPickingShader->Bind();

			if (drawCommand.Targetable) 
			{
				if (drawCommand.Type == ElementType::Panel)
				{
					DirectX::XMVECTOR vScale, vTransform, vRotation;
					DirectX::XMMatrixDecompose(&vScale, &vTransform, &vRotation, drawCommand.Transform);
					drawCommand.Element->SetWidth(DirectX::XMVectorGetX(vScale));
					drawCommand.Element->SetHeight(DirectX::XMVectorGetY(vScale));

					Ref<UIPanel> panel = std::dynamic_pointer_cast<UIPanel>(drawCommand.Element);
					panel->Bind();

					RenderCommand::DrawIndexed(0, 0, 6);
				}
				if (drawCommand.Type == ElementType::Button)
				{
					DirectX::XMVECTOR vScale, vTransform, vRotation;
					DirectX::XMMatrixDecompose(&vScale, &vTransform, &vRotation, drawCommand.Transform);
					drawCommand.Element->SetWidth(DirectX::XMVectorGetX(vScale));
					drawCommand.Element->SetHeight(DirectX::XMVectorGetY(vScale));

					Ref<UIButton> button = std::dynamic_pointer_cast<UIButton>(drawCommand.Element);
					button->Bind();

					RenderCommand::DrawIndexed(0, 0, 6);
				}
				if (drawCommand.Type == ElementType::Text)
				{
					Ref<UIText> text = std::dynamic_pointer_cast<UIText>(drawCommand.Element);
					text->Bind();

					RenderCommand::DrawIndexed(0, 0, text->GetQuadCount());
				}
			}
		}

		ClearDrawList();
		
		RenderCommand::BindBackbuffer();
		RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
	}

	void Renderer2D::ClearDrawList()
	{
		sRenderer2DData->ElementDrawList.clear();
	}

	void Renderer2D::SubmitPanel(const DirectX::XMMATRIX& transform, const Ref<UIPanel>& panel, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(panel, transform, ElementType::Panel, entityID, targetable);
	}

	void Renderer2D::SubmitButton(const DirectX::XMMATRIX& transform, const Ref<UIButton>& button, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(button, transform, ElementType::Button, entityID, targetable);
	}

	void Renderer2D::SubmitText(const DirectX::XMMATRIX& transform, const Ref<UIText>& text, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(text, transform, ElementType::Text, entityID, targetable);
	}
}