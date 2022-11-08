#pragma once

#include "Toast/Renderer/OrthographicCamera.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Camera.h"

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class Renderer2D : Renderer
	{
	private:
		enum ElementType 
		{
			Panel = 0,
			Text = 1,
			Button = 2
		};

		struct DrawCommand
		{
		public:
			DrawCommand(const Ref<UIElement> element, const DirectX::XMMATRIX& transform, ElementType type, const int entityID = 0, const bool targetable = false)
				: Element(element), Transform(transform), Type(type), EntityID(entityID), Targetable(targetable) {}
		public:
			Ref<UIElement> Element;
			DirectX::XMMATRIX Transform;
			ElementType Type;
			const int EntityID;
			const bool Targetable;
		};

		struct QuadVertex
		{
			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT4 Color;
			DirectX::XMFLOAT2 TexCoord;
		};

		struct Renderer2DData
		{
			std::vector<DrawCommand> ElementDrawList;
		};

	protected:
		static Scope<Renderer2DData> sRenderer2DData;

	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(Camera& camera);
		static void EndScene();

		static void ClearDrawList();

		static void SubmitPanel(const DirectX::XMMATRIX& transform, const Ref<UIPanel>& panel, const int entityID, const bool targetable);
		static void SubmitButton(const DirectX::XMMATRIX& transform, const Ref<UIButton>& button, const int entityID, const bool targetable);
		static void SubmitText(const DirectX::XMMATRIX& transform, const Ref<UIText>& text, const int entityID, const bool targetable);
	};
}