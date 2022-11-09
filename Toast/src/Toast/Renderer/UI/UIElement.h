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

		template <typename T>
		void Set(const std::string& cbufferName, const std::string& name, const T& value)
		{
			auto decl = FindCBufferElementDeclaration(cbufferName, name);

			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");
			if (!decl)
				return;

			mModelBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
		}

		template <typename T>
		T& Get(const std::string& cbufferName, const std::string& name)
		{
			auto decl = FindCBufferElementDeclaration(bufferName, name);
			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");

			return mModelBuffer.Read<T>(decl->GetOffset());
		}

		void SetWidth(float w) { mWidth = w; }
		float* GetWidth() { return &mWidth; }
		void SetHeight(float h) { mHeight = h; }
		float* GetHeight() { return &mHeight; }

		void SetColor(DirectX::XMFLOAT4 c) { mColor = c; }
		float* GetColor() { return &mColor.x; }
		DirectX::XMFLOAT4 GetColorF4() { return mColor; }

		void Bind();

		uint32_t GetQuadCount() { return mQuadIndexCount; }
	private:
		const ShaderCBufferElement* FindCBufferElementDeclaration(const std::string& cbufferName, const std::string& name);
	public:
		Shader* mShader;
		Shader* mPickingShader;
	private:
		DirectX::XMFLOAT4 mColor = { 1.0f, 1.0f, 1.0f, 1.0f };;

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
	protected:
		float mCornerRadius = 0.0f;
	};

	class UIText : public UIElement 
	{
	public:
		UIText();
		~UIText() = default;

		void Bind();

		void SetText(std::string& str) { mTextString = str; InvalidateText(); }
		std::string& GetText() { return mTextString; }

		void SetFont(Ref<Font>& f) { mTextFont = f; }
		Ref<Font> GetFont() { return mTextFont; }

		void InvalidateText();
	private:
		std::string mTextString = "Enter Text here";
		Ref<Font> mTextFont;
	};

	class UIButton : public UIPanel
	{
	public:
		UIButton();
		~UIButton() = default;

		void Bind();

		float* GetClickColor() { return &mClickColor.x; }
		DirectX::XMFLOAT4 GetClickColorF4() { return mClickColor; }
		void SetClickColor(DirectX::XMFLOAT4 c) { mClickColor = c; }
	private:
		DirectX::XMFLOAT4 mClickColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
}