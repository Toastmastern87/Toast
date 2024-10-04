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

		sRenderer2DData->UIVertexBufferBase = new UIVertex[sRenderer2DData->MaxUIVertices];
		sRenderer2DData->UIVertexBufferPtr = sRenderer2DData->UIVertexBufferBase;
		uint32_t* UIIndices = new uint32_t[sRenderer2DData->MaxUIIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < sRenderer2DData->MaxUIIndices; i += 6)
		{
			UIIndices[i + 0] = offset + 0;
			UIIndices[i + 1] = offset + 2;
			UIIndices[i + 2] = offset + 1;
			
			UIIndices[i + 3] = offset + 2;
			UIIndices[i + 4] = offset + 0;
			UIIndices[i + 5] = offset + 3;

			offset += 4;
		}

		sRenderer2DData->UIVertexBuffer = CreateRef<VertexBuffer>(&sRenderer2DData->UIVertexBufferBase[0], (uint32_t)(sRenderer2DData->MaxUIVertices * sizeof(UIVertex)), sRenderer2DData->MaxUIVertices, 0, D3D11_USAGE_DYNAMIC);
		sRenderer2DData->UIIndexBuffer = CreateRef<IndexBuffer>(&UIIndices[0], sRenderer2DData->MaxUIIndices);
	}

	void Renderer2D::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void Renderer2D::BeginScene(Camera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::SetPrimitiveTopology(PrimitiveTopology::TRIANGLELIST);

		auto[width, height] = sRendererData->FinalRenderTarget->GetSize();

		float fWidth, fHeight;
		fWidth = (float)width;
		fHeight = (float)height;

		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 0);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetOrthoProjection(), 64, 64);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		sRenderer2DData->UIVertexBufferPtr = sRenderer2DData->UIVertexBufferBase;
	}

	void Renderer2D::EndScene()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"2D Render Pass");
#endif

		sRendererData->FinalFramebuffer->DisableDepth();
		sRendererData->FinalFramebuffer->Bind();

		// New way of rendering UI with one big vertex buffer
		uint32_t vertexCount = sRenderer2DData->UIVertexBufferPtr - sRenderer2DData->UIVertexBufferBase;
		uint32_t vertexDataSize = vertexCount * sizeof(UIVertex);
		uint32_t quadCount = vertexCount / 4; 
		uint32_t indexCount = quadCount * 6;

		ShaderLibrary::Get("assets/shaders/UI.hlsl")->Bind();
		sRenderer2DData->UIVertexBuffer->SetData(sRenderer2DData->UIVertexBufferBase, vertexDataSize);
		sRenderer2DData->UIVertexBuffer->Bind();
		sRenderer2DData->UIIndexBuffer->Bind();
		RenderCommand::DrawIndexed(0, 0, indexCount);

		/*for (const auto& drawCommand : sRenderer2DData->ElementDrawList)
		{
			drawCommand.Element->Set<int>("Model", "entityID", drawCommand.EntityID);

			if (sRenderer2DData->shaderNameBound != drawCommand.Element->mShader->GetName())
			{
				drawCommand.Element->mShader->Bind();
				sRenderer2DData->shaderNameBound = drawCommand.Element->mShader->GetName();
			}

			if (drawCommand.Type == ElementType::Panel) 
			{
				drawCommand.Element->Set("UIProp", "size", drawCommand.Size);

				Ref<UIPanel> panel = std::dynamic_pointer_cast<UIPanel>(drawCommand.Element);
				if (!sRenderer2DData->UIBuffersBound)
				{
					panel->Bind();
					sRenderer2DData->UIBuffersBound = true;
				}
				else
					panel->Map();

				RenderCommand::DrawIndexed(0, 0, 6);
			}
			if (drawCommand.Type == ElementType::Button)
			{
				drawCommand.Element->Set("UIProp", "size", drawCommand.Size);
				
				Ref<UIButton> button = std::dynamic_pointer_cast<UIButton>(drawCommand.Element);
				if (!sRenderer2DData->UIBuffersBound)
				{
					button->Bind();
					sRenderer2DData->UIBuffersBound = true;
				}
				else
					button->Map();

				RenderCommand::DrawIndexed(0, 0, 6);
			}
			if (drawCommand.Type == ElementType::Text)
			{
				Ref<UIText> text = std::dynamic_pointer_cast<UIText>(drawCommand.Element);
				if (!sRenderer2DData->UITextBuffersBound)
				{
					text->Bind();
					sRenderer2DData->UITextBuffersBound = true;
				}
				else
					text->Map();

				RenderCommand::DrawIndexed(0, 0, text->GetQuadCount());
			}
		}*/

		sRendererData->FinalFramebuffer->EnableDepth();

		// Picking
		sRendererData->PickingFramebuffer->Bind();
		RenderCommand::DisableBlending();

		/*for (const auto& drawCommand : sRenderer2DData->ElementDrawList)
		{
			if (sRenderer2DData->shaderNameBound != drawCommand.Element->mPickingShader->GetName())
			{
				drawCommand.Element->mPickingShader->Bind();
				sRenderer2DData->shaderNameBound = drawCommand.Element->mPickingShader->GetName();
			}

			if (drawCommand.Targetable) 
			{
				if (drawCommand.Type == ElementType::Panel)
				{
					drawCommand.Element->Set("UIProp", "size", drawCommand.Size);

					Ref<UIPanel> panel = std::dynamic_pointer_cast<UIPanel>(drawCommand.Element);
					if (!sRenderer2DData->UIBuffersBound)
					{
						panel->Bind();
						sRenderer2DData->UIBuffersBound = true;
					}
					else
						panel->Map();

					RenderCommand::DrawIndexed(0, 0, 6);
				}
				if (drawCommand.Type == ElementType::Button)
				{
					drawCommand.Element->Set("UIProp", "size", drawCommand.Size);

					Ref<UIButton> button = std::dynamic_pointer_cast<UIButton>(drawCommand.Element);
					if (!sRenderer2DData->UIBuffersBound)
					{
						button->Bind();
						sRenderer2DData->UIBuffersBound = true;
					}
					else
						button->Map();

					RenderCommand::DrawIndexed(0, 0, 6);
				}
				if (drawCommand.Type == ElementType::Text)
				{
					Ref<UIText> text = std::dynamic_pointer_cast<UIText>(drawCommand.Element);
					if (!sRenderer2DData->UITextBuffersBound)
					{
						text->Bind();
						sRenderer2DData->UITextBuffersBound = true;
					}
					else
						text->Map();

					RenderCommand::DrawIndexed(0, 0, text->GetQuadCount());
				}
			}
		}*/

		ClearDrawList();
		
		RenderCommand::BindBackbuffer();
		RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer2D::ClearDrawList()
	{
		sRenderer2DData->ElementDrawList.clear();
	}

	void Renderer2D::SubmitPanel(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, DirectX::XMFLOAT4& color, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMFLOAT3 UIVertexPositions[4];

		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };

		UIVertexPositions[0] = DirectX::XMFLOAT3(pos.x, pos.y, 1.0f);
		UIVertexPositions[1] = DirectX::XMFLOAT3(pos.x + size.x, pos.y, 1.0f);
		UIVertexPositions[2] = DirectX::XMFLOAT3(pos.x + size.x, pos.y + size.y, 1.0f);
		UIVertexPositions[3] = DirectX::XMFLOAT3(pos.x, pos.y + size.y, 1.0f);

		for (size_t i = 0; i < 4; i++)
		{
			sRenderer2DData->UIVertexBufferPtr->Position = UIVertexPositions[i];
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Color = color;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = textureCoords[i];
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr++;
		}
	}

	void Renderer2D::SubmitButton(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<UIButton>& button, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(button, pos, size, ElementType::Button, entityID, targetable);
	}

	void Renderer2D::SubmitText(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<UIText>& text, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->ElementDrawList.emplace_back(text, pos, size, ElementType::Text, entityID, targetable);
	}
}