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

		Ref<Scene> mActiveScene;
		Entity mSquareEntity;
		Entity mCameraEntity;
		Entity mSecondCamera;

		bool mPrimaryCamera = true;

		Ref<Texture2D> mCheckerboardTexture;

		bool mViewportFocused = false, mViewportHovered = false;

		DirectX::XMFLOAT2 mViewportSize = { 0.0f, 0.0f };
	};
}