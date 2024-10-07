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

	////////////////////////////////////////////////////////////////////////////////////////  
	//      UIBUTTON        ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIButton::UIButton() 
	{
	}

}
