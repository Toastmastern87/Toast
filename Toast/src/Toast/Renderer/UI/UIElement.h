#pragma once

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"

#include <DirectXMath.h>

namespace Toast {

	struct UIVertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 Texcoord;

		UIVertex() = default;

		UIVertex(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT2 uv)
		{
			Position = pos;
			Texcoord = uv;
		}
	};

	class UIElement
	{
	public:
		UIElement();
		virtual ~UIElement() = default;

		void SetWidth(float w) { mWidth = w; }
		float* GetWidth() { return &mWidth; }
		void SetHeight(float h) { mHeight = h; }
		float* GetHeight() { return &mHeight; }

		void SetColor(DirectX::XMFLOAT4 c) { mColor = c; }
		float* GetColor() { return &mColor.x; }
		DirectX::XMFLOAT4 GetColorF4() { return mColor; }

		void Bind();

		void Transform(DirectX::XMMATRIX transform);

	private:
		float mWidth;
		float mHeight;
		DirectX::XMFLOAT4 mColor;

		Ref<VertexBuffer> mVertexBuffer;
		Ref<IndexBuffer> mIndexBuffer;

		UIVertex mVertices[4];
		uint32_t mIndices[6];

	protected:
		Ref<ConstantBuffer> mUIPropCBuffer;
		Buffer mUIPropBuffer;
	};

	class UIPanel : public UIElement 
	{
	public:
		UIPanel();
		~UIPanel() = default;

		void Bind();

		float* GetCornerRadius() { return &mCornerRadius; }
		void SetCornerRadius(float radius) { mCornerRadius = radius; }
	private:
		float mCornerRadius = 0.0f;
	};
}