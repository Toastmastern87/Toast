#include "tpch.h"
#include "UIElement.h"

#include "Toast/Renderer/UI/MSDFData.h"

#include <codecvt>

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//      UIELEMENT       ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIElement::UIElement()
	{
		mQuadVertexBufferBase = new UIVertex[mMaxVertices];
		mQuadVertexBufferPtr = mQuadVertexBufferBase;
		mQuadIndexCount = 0;

		uint32_t* quadIndices = new uint32_t[mMaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < mMaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 2;
			quadIndices[i + 2] = offset + 1;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 0;
			quadIndices[i + 5] = offset + 3;

			offset += 4;
		}

		mVertexBuffer = CreateRef<VertexBuffer>(&mQuadVertexBufferBase[0], (uint32_t)(mMaxVertices * sizeof(UIVertex)), (uint32_t)mMaxVertices, 0, D3D11_USAGE_DYNAMIC);
		mIndexBuffer = CreateRef<IndexBuffer>(&quadIndices[0], (uint32_t)mMaxIndices);

		delete[] quadIndices;

		mUIPropCBuffer = ConstantBufferLibrary::Load("UIProp", 32, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, 13) });
		mUIPropCBuffer->Bind();
		mUIPropBuffer.Allocate(mUIPropCBuffer->GetSize());
		mUIPropBuffer.ZeroInitialize();

		mModelCBuffer = ConstantBufferLibrary::Load("Model", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 1) });
		mModelCBuffer->Bind();
		mModelBuffer.Allocate(mModelCBuffer->GetSize());
		mModelBuffer.ZeroInitialize();
	}

	void UIElement::Bind()
	{
		uint32_t dataSize = (uint32_t)((uint8_t*)mQuadVertexBufferPtr - (uint8_t*)mQuadVertexBufferBase);
		mVertexBuffer->SetData(mQuadVertexBufferBase, dataSize);

		mVertexBuffer->Bind();
		mIndexBuffer->Bind();

		mUIPropBuffer.Write((void*)&mColor, 16, 0);
		mUIPropCBuffer->Map(mUIPropBuffer);
		mUIPropCBuffer->Bind();

		mModelBuffer.Write((byte*)&mTransform, 64, 0);
		mModelCBuffer->Map(mModelBuffer);
		mModelCBuffer->Bind();
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//       UIPANEL        ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIPanel::UIPanel()
	{
		constexpr size_t quadVertexCount = 4;
		constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };

		mQuadVertexPositions[0] = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
		mQuadVertexPositions[1] = DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f);
		mQuadVertexPositions[2] = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
		mQuadVertexPositions[3] = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			mQuadVertexBufferPtr->Position = mQuadVertexPositions[i];
			mQuadVertexBufferPtr->Texcoord = textureCoords[i];
			mQuadVertexBufferPtr++;
		}

		mQuadIndexCount = 6;
	}

	void UIPanel::Bind()
	{
		float type = 1.0f;

		mUIPropBuffer.Write((void*)&mCornerRadius, 4, 24);
		mUIPropBuffer.Write((void*)&mWidth, 4, 16);
		mUIPropBuffer.Write((void*)&mHeight, 4, 20);
		mUIPropBuffer.Write((void*)&type, 4, 28);

		UIElement::Bind();
	}
	
	////////////////////////////////////////////////////////////////////////////////////////  
	//       UITEXT         ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIText::UIText()
	{
		TextFont = Font::GetDefaultFont();
	}

	void UIText::Bind()
	{
		float type = 2.0f;

		mUIPropBuffer.Write((void*)&type, 4, 28);

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		TextFont->GetFontAtlas()->Bind(6, D3D11_PIXEL_SHADER);

		UIElement::Bind();
	}

	void UIText::SetText(std::string& str)
	{
		TextString = str;

		if (TextString.empty())
			return;

		mQuadVertexBufferPtr = mQuadVertexBufferBase;
		mQuadIndexCount = 0;

		Ref<Texture2D> texAtlas = TextFont->GetFontAtlas();
		TOAST_CORE_ASSERT(texAtlas, "");

		auto& fontGeometry = TextFont->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();

		double x = 0.0f;
		double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
		double y = 0;
		for (int i = 0; i < TextString.size(); i++)
		{
			char32_t character = TextString[i];
			// New row
			if (character == '\n')
			{
				x = 0;
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
			
			mQuadVertexBufferPtr->Position = { (float)pl, (float)pb, 0.0f };
			mQuadVertexBufferPtr->Texcoord = { (float)l, (float)b };
			mQuadVertexBufferPtr++;

			mQuadVertexBufferPtr->Position = { (float)pl, (float)pt, 0.0f };
			mQuadVertexBufferPtr->Texcoord = { (float)l, (float)t };
			mQuadVertexBufferPtr++;

			mQuadVertexBufferPtr->Position = { (float)pr, (float)pt, 0.0f };
			mQuadVertexBufferPtr->Texcoord = { (float)r, (float)t };
			mQuadVertexBufferPtr++;

			mQuadVertexBufferPtr->Position = { (float)pr, (float)pb, 0.0f };
			mQuadVertexBufferPtr->Texcoord = { (float)r, (float)b };
			mQuadVertexBufferPtr++;

			mQuadIndexCount += 6;

			double advance = glyph->getAdvance();
			fontGeometry.getAdvance(advance, character, TextString[i + 1]);
			x += fsScale * advance;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//      UIBUTTON        ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIButton::UIButton() 
	{
	}

	void UIButton::Bind()
	{

	}

}
