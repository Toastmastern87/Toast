#pragma once

#include <Toast.h>

namespace Toast {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& e) override;
	private:
		OrthographicCameraController mCameraController;

		Ref<Framebuffer> mFramebuffer;

		Ref<Texture2D> mCheckerboardTexture;

		float mSquareColor[4] = { 0.8f, 0.2f, 0.3f, 1.0f };
	};
}