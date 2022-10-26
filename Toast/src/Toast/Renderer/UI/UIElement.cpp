#include "tpch.h"
#include "UIElement.h"

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//      UIELEMENT       ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIElement::UIElement()
	{
		mVertices[0] = UIVertex(DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f));
		mVertices[1] = UIVertex(DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f));
		mVertices[2] = UIVertex(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f));
		mVertices[3] = UIVertex(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f));

		mIndices[0] = 0;
		mIndices[1] = 2;
		mIndices[2] = 1;

		mIndices[3] = 2;
		mIndices[4] = 0;
		mIndices[5] = 3;

		mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (uint32_t)sizeof(mVertices), (uint32_t)std::size(mVertices), 0, D3D11_USAGE_DYNAMIC);
		mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)sizeof(mIndices));

		mUIPropCBuffer = ConstantBufferLibrary::Load("UIProp", 32, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, 13) });
		mUIPropCBuffer->Bind();
		mUIPropBuffer.Allocate(mUIPropCBuffer->GetSize());
		mUIPropBuffer.ZeroInitialize();
	}

	void UIElement::Bind()
	{
		mVertexBuffer->SetData(&mVertices[0], (uint32_t)sizeof(mVertices));
		mVertexBuffer->Bind();
		mIndexBuffer->Bind();

		mUIPropBuffer.Write((void*)&mColor, 16, 0);
		mUIPropBuffer.Write((void*)&mWidth, 4, 16);
		mUIPropBuffer.Write((void*)&mHeight, 4, 20);
		mUIPropCBuffer->Map(mUIPropBuffer);
		mUIPropCBuffer->Bind();
	}

	void UIElement::Transform(DirectX::XMMATRIX transform)
	{
		mVertices[0].Position = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
		mVertices[1].Position = DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f);
		mVertices[2].Position = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
		mVertices[3].Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

		for (size_t i = 0; i < std::size(mVertices); i++) 
			DirectX::XMStoreFloat3(&mVertices[i].Position, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mVertices[i].Position), transform));
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//       UIPANEL        ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	UIPanel::UIPanel()
	{

	}

	void UIPanel::Bind()
	{
		mUIPropBuffer.Write((void*)&mCornerRadius, 4, 24);

		UIElement::Bind();
	}

}
