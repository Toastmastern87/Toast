#pragma once

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/UI/Font.h"

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

		void SetTransform(DirectX::XMMATRIX transform) { mTransform = transform; }

		uint32_t GetQuadCount() { return mQuadIndexCount; }

	private:
		DirectX::XMFLOAT4 mColor;

		Ref<VertexBuffer> mVertexBuffer;
		Ref<IndexBuffer> mIndexBuffer;

	protected:
		float mWidth, mHeight;

		Ref<ConstantBuffer> mUIPropCBuffer, mModelCBuffer;
		Buffer mUIPropBuffer, mModelBuffer;

		const uint32_t mMaxQuads = 256;
		const uint32_t mMaxVertices = mMaxQuads * 4;
		const uint32_t mMaxIndices = mMaxQuads * 6;

		uint32_t mQuadIndexCount = 0;
		UIVertex* mQuadVertexBufferBase = nullptr;
		UIVertex* mQuadVertexBufferPtr = nullptr;

		DirectX::XMFLOAT3 mQuadVertexPositions[4];

		DirectX::XMMATRIX mTransform;
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

	class UIText : public UIElement 
	{
	public:
		UIText();
		~UIText() = default;

		void Bind();

		void SetText(std::string& str);
		std::string& GetText() { return TextString; }

		void SetFont(Ref<Font>& f) { TextFont = f; }
		Ref<Font> GetFont() { return TextFont; }
	private:
		std::string TextString = "Enter Text here";
		Ref<Font> TextFont;
	};

	class UIButton : public UIElement
	{
	public:
		UIButton();
		~UIButton() = default;

		void Bind();

		void SetText(Ref<UIText>& text) { Text = text; }
		Ref<UIText>& GetText() { return Text; }

	private:
		Ref<UIText> Text;
	};
}