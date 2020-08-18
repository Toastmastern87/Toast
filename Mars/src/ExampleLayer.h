#pragma once

#include "Toast.h"

class ExampleLayer : public Toast::Layer
{
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(Toast::Timestep ts) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Toast::Event& e) override;
private:
	Toast::ShaderLibrary mShaderLibrary;
	Toast::Ref<Toast::BufferLayout> mBufferLayout, mTextureBufferLayout;
	Toast::Ref<Toast::VertexBuffer> mVertexBuffer;
	Toast::Ref<Toast::IndexBuffer> mIndexBuffer;

	Toast::Ref<Toast::Texture2D> mTexture, mMarsLogoTexture;

	Toast::OrthographicCameraController mCameraController;

	float mSquareColor[3] = { 0.8f, 0.2f, 0.3f };
};