#pragma once

#include <Toast.h>

class TheNextFrontier2D : public Toast::Layer
{
public:
	TheNextFrontier2D();
	virtual ~TheNextFrontier2D() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(Toast::Timestep ts) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Toast::Event& e) override;
private:
	Toast::OrthographicCameraController mCameraController;

	Toast::Ref<Toast::Shader> mFlatColorShader;
	Toast::Ref<Toast::BufferLayout> mBufferLayout;
	Toast::Ref<Toast::VertexBuffer> mVertexBuffer;
	Toast::Ref<Toast::IndexBuffer> mIndexBuffer;

	float mSquareColor[4] = { 0.8f, 0.2f, 0.3f, 1.0f };
};