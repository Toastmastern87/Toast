#pragma once

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/UI/Font.h"

#include <DirectXMath.h>

namespace Toast {

	struct UIVertex
	{
		DirectX::XMFLOAT4 Position; // z component holds what type of UI Element this is, w component holds if the element is textured or not
		DirectX::XMFLOAT4 Size; // z component holds the corner radius for a panel, w component holds the size of the border
		DirectX::XMFLOAT4 Color;
		DirectX::XMFLOAT2 Texcoord;
		uint32_t EntityID;

		UIVertex() = default;

		UIVertex(DirectX::XMFLOAT4 pos, DirectX::XMFLOAT4 size, DirectX::XMFLOAT4 color, DirectX::XMFLOAT2 uv, uint32_t id)
		{
			Position = pos;
			Size = size;
			Color = color;
			Texcoord = uv;
			EntityID = id;
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

		bool GetVisible() { return mVisible; }
		void SetVisible(bool visible) { mVisible = visible; }

		void SetColor(DirectX::XMFLOAT4 c) { mColor = c; }
		float* GetColor() { return &mColor.x; }
		DirectX::XMFLOAT4 GetColorF4() { return mColor; }
	private:
		bool mVisible = false;

		DirectX::XMFLOAT4 mColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	};

	class UIPanel : public UIElement 
	{
	public:
		UIPanel();
		UIPanel(float posX, float posY, float width, float height);
		~UIPanel() = default;

		float* GetCornerRadius() { return &mCornerRadius; }
		void SetCornerRadius(float radius) { mCornerRadius = radius; }

		float* GetBorderSize() { return &mBorderSize; }
		void SetBorderSize(float size) { mBorderSize = size; }

		void SetUseColor(bool useColor) { mUseColor = useColor; }
		bool GetUseColor() { return mUseColor; }
		void SetTextureFilepath(std::string& textureFilepath) { mTextureFilepath = textureFilepath; }
		std::string& GetTextureFilepath() { return mTextureFilepath; }

		bool GetConnectToParent() { return mConnectToParent; }
		void SetConnectToParent(bool connectToParent) { mConnectToParent = connectToParent; }
	protected:
		float mCornerRadius = 0.0f;
		float mBorderSize = 0.0f;

		bool mUseColor = true;
		std::string mTextureFilepath;

		bool mConnectToParent = false;
	};

	class UIText : public UIElement 
	{
	public:
		UIText();
		~UIText() = default;

		void SetText(std::string& str) { mTextString = str; }
		std::string& GetText() { return mTextString; }

		void SetFont(Ref<Font>& f) { mTextFont = f; }
		Ref<Font> GetFont() { return mTextFont; }
	private:
		std::string mTextString = "Enter Text here";
		Ref<Font> mTextFont;
	};

	class UIButton : public UIPanel
	{
	public:
		UIButton();
		~UIButton() = default;

		float* GetClickColor() { return &mClickColor.x; }
		DirectX::XMFLOAT4 GetClickColorF4() { return mClickColor; }
		void SetClickColor(DirectX::XMFLOAT4 c) { mClickColor = c; }
	private:
		DirectX::XMFLOAT4 mClickColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
}