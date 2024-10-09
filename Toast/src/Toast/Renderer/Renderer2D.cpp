#include "tpch.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/UI/Font.h"
#include "Toast/Renderer/UI/MSDFData.h"

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
		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		if(sRenderer2DData->TextFont)
			sRenderer2DData->TextFont->GetFontAtlas()->Bind(6, D3D11_PIXEL_SHADER);
		if (sRenderer2DData->PanelTextureName != "")
			TextureLibrary::Get(sRenderer2DData->PanelTextureName)->Bind(8, D3D11_PIXEL_SHADER);
		sRenderer2DData->UIVertexBuffer->SetData(sRenderer2DData->UIVertexBufferBase, vertexDataSize);
		sRenderer2DData->UIVertexBuffer->Bind();
		sRenderer2DData->UIIndexBuffer->Bind();
		RenderCommand::DrawIndexed(0, 0, indexCount);

		sRendererData->FinalFramebuffer->EnableDepth();
		
		RenderCommand::BindBackbuffer();
		RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer2D::SubmitPanel(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, DirectX::XMFLOAT4& color, const int entityID, const bool textured, std::string panelTextureName, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMFLOAT4 UIVertexPositions[4];

		float texturedF = textured == true ? 1.0f : 0.0f;

		if (textured)
			sRenderer2DData->PanelTextureName = panelTextureName;

		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };

		UIVertexPositions[0] = DirectX::XMFLOAT4(pos.x, pos.y, 1.0f, texturedF);
		UIVertexPositions[1] = DirectX::XMFLOAT4(pos.x + size.x, pos.y, 1.0f, texturedF);
		UIVertexPositions[2] = DirectX::XMFLOAT4(pos.x + size.x, pos.y + size.y, 1.0f, texturedF);
		UIVertexPositions[3] = DirectX::XMFLOAT4(pos.x, pos.y + size.y, 1.0f, texturedF);

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

	void Renderer2D::SubmitButton(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, DirectX::XMFLOAT4& color, const int entityID, const bool textured, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMFLOAT4 UIVertexPositions[4];

		float texturedF = textured == true ? 1.0f : 0.0f;

		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };

		UIVertexPositions[0] = DirectX::XMFLOAT4(pos.x, pos.y, 1.0f, texturedF);
		UIVertexPositions[1] = DirectX::XMFLOAT4(pos.x + size.x, pos.y, 1.0f, texturedF);
		UIVertexPositions[2] = DirectX::XMFLOAT4(pos.x + size.x, pos.y + size.y, 1.0f, texturedF);
		UIVertexPositions[3] = DirectX::XMFLOAT4(pos.x, pos.y + size.y, 1.0f, texturedF);

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

	void Renderer2D::SubmitText(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, const Ref<UIText>& text, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		std::string& textString = text->GetText();
		Ref<Font> textFont = text->GetFont();

		sRenderer2DData->TextFont = textFont;

		if (textString.empty())
			return;

		Ref<Texture2D> texAtlas = textFont->GetFontAtlas();
		TOAST_CORE_ASSERT(texAtlas, "");

		auto& fontGeometry = textFont->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();

		// Calculate font scale based on desired size
		double fontHeight = metrics.ascenderY - metrics.descenderY;
		double fsScale = 1 / fontHeight;

		fsScale *= size.y;

		// Initialize positions
		double x = pos.x;
		double y = pos.y + fsScale * (-metrics.descenderY); // Adjust y to align baseline

		for (int i = 0; i < textString.size(); i++)
		{
			char32_t character = textString[i];
			// New row
			if (character == '\n')
			{
				x = pos.x;
				y -= fsScale * metrics.lineHeight;
				continue;
			}

			auto glyph = fontGeometry.getGlyph(character);
			if (!glyph)
				glyph = fontGeometry.getGlyph('?');
			if (!glyph)
				continue;

			double l, b, r, t;
			glyph->getQuadAtlasBounds(l, b, r, t);

			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);

			pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
			pl += x, pb += y, pr += x, pt += y;

			double texelWidth = 1. / texAtlas->GetWidth();
			double texelHeight = 1. / texAtlas->GetHeight();
			l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

			// Set vertex data
			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pl, (float)pb, pos.z, 0.0f }; // Bottom-Left
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)l, (float)b };
			sRenderer2DData->UIVertexBufferPtr->Color = text->GetColorF4();// Assuming text has a color
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr++;

			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pr, (float)pb, pos.z, 0.0f }; // Bottom-Right
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)r, (float)b };
			sRenderer2DData->UIVertexBufferPtr->Color = text->GetColorF4();
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr++;

			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pr, (float)pt, pos.z, 0.0f }; // Top-Right
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)r, (float)t };
			sRenderer2DData->UIVertexBufferPtr->Color = text->GetColorF4();
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr++;

			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pl, (float)pt, pos.z, 0.0f }; // Top-Left
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)l, (float)t };
			sRenderer2DData->UIVertexBufferPtr->Color = text->GetColorF4();
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr++;

			double advance = glyph->getAdvance();
			fontGeometry.getAdvance(advance, character, textString[i + 1]);
			x += fsScale * advance;
		 }
	}
}