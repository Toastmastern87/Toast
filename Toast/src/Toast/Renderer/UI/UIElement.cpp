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

	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//       UIPANEL        ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIPanel::UIPanel()
	{
		//constexpr size_t quadVertexCount = 4;
		//constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };

		//mQuadVertexPositions[0] = DirectX::XMFLOAT2(0.0f, -1.0f);
		//mQuadVertexPositions[1] = DirectX::XMFLOAT2(1.0f, -1.0f);
		//mQuadVertexPositions[2] = DirectX::XMFLOAT2(1.0f, 0.0f);
		//mQuadVertexPositions[3] = DirectX::XMFLOAT2(0.0f, 0.0f);

		//for (size_t i = 0; i < quadVertexCount; i++)
		//{
		//	mQuadVertexBufferPtr->Position = mQuadVertexPositions[i];
		//	mQuadVertexBufferPtr->Texcoord = textureCoords[i];
		//	mQuadVertexBufferPtr++;
		//}

		//mQuadIndexCount = 6;
	}

	UIPanel::UIPanel(float posX, float posY, float width, float height)
	{
		//UIElement(posX, posY, width, height);

		//constexpr size_t quadVertexCount = 4;
		//constexpr DirectX::XMFLOAT2 textureCoords[] = { DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };

		//mQuadVertexPositions[0] = DirectX::XMFLOAT2(posX, posY);
		//mQuadVertexPositions[1] = DirectX::XMFLOAT2(posX + width, posY);
		//mQuadVertexPositions[2] = DirectX::XMFLOAT2(posX + width, posY+height);
		//mQuadVertexPositions[3] = DirectX::XMFLOAT2(posX, posY + height);

		//for (size_t i = 0; i < quadVertexCount; i++)
		//{
		//	mQuadVertexBufferPtr->Position = mQuadVertexPositions[i];
		//	mQuadVertexBufferPtr->Texcoord = textureCoords[i];
		//	mQuadVertexBufferPtr++;
		//}

		//mQuadIndexCount = 6;
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//       UITEXT         ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIText::UIText()
	{
		mTextFont = Font::GetDefaultFont();
	}

	void UIText::InvalidateText()
	{
		//if (mTextString.empty())
		//	return;

		//mQuadVertexBufferPtr = mQuadVertexBufferBase;
		//mQuadIndexCount = 0;

		//Ref<Texture2D> texAtlas = mTextFont->GetFontAtlas();
		//TOAST_CORE_ASSERT(texAtlas, "");

		//auto& fontGeometry = mTextFont->GetMSDFData()->FontGeometry;
		//const auto& metrics = fontGeometry.getMetrics();

		//double x = 0.0f;
		//double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
		//double y = 0;
		//for (int i = 0; i < mTextString.size(); i++)
		//{
		//	char32_t character = mTextString[i];
		//	// New row
		//	if (character == '\n')
		//	{
		//		x = 0;
		//		y -= fsScale * metrics.lineHeight;
		//		continue;
		//	}

		//	auto glyph = fontGeometry.getGlyph(character);
		//	if (!glyph)
		//		glyph = fontGeometry.getGlyph('?');
		//	if (!glyph)
		//		continue;

		//	double l, b, r, t;
		//	glyph->getQuadAtlasBounds(l, b, r, t);

		//	double pl, pb, pr, pt;
		//	glyph->getQuadPlaneBounds(pl, pb, pr, pt);

		//	pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
		//	pl += x, pb += y, pr += x, pt += y;

		//	double texelWidth = 1. / texAtlas->GetWidth();
		//	double texelHeight = 1. / texAtlas->GetHeight();
		//	l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

		//	mQuadVertexBufferPtr->Position = { (float)pl, (float)pb };
		//	mQuadVertexBufferPtr->Texcoord = { (float)l, (float)b };
		//	mQuadVertexBufferPtr++;

		//	mQuadVertexBufferPtr->Position = { (float)pl, (float)pt };
		//	mQuadVertexBufferPtr->Texcoord = { (float)l, (float)t };
		//	mQuadVertexBufferPtr++;

		//	mQuadVertexBufferPtr->Position = { (float)pr, (float)pt };
		//	mQuadVertexBufferPtr->Texcoord = { (float)r, (float)t };
		//	mQuadVertexBufferPtr++;

		//	mQuadVertexBufferPtr->Position = { (float)pr, (float)pb };
		//	mQuadVertexBufferPtr->Texcoord = { (float)r, (float)b };
		//	mQuadVertexBufferPtr++;

		//	mQuadIndexCount += 6;

		//	double advance = glyph->getAdvance();
		//	fontGeometry.getAdvance(advance, character, mTextString[i + 1]);
		//	x += fsScale * advance;
		// }
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//      UIBUTTON        ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIButton::UIButton() 
	{
	}

}
