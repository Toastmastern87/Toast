#include <Toast.h>

#include "imgui/imgui.h"

class ExampleLayer : public Toast::Layer 
{
public:
	ExampleLayer()
		: Layer("Example"), mCamera(-1.6f, 1.6f, 0.9f, -0.9f), mCameraPosition(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f))
	{
		float vertices[3 * 7] = {
								 -0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
								 0.0f, 0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f,
								 0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f
		};

		mVertexBuffer.reset(Toast::VertexBuffer::Create(vertices, sizeof(vertices), uint32_t(3)));

		uint32_t indices[3] = { 0, 1, 2 };

		mIndexBuffer.reset(Toast::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));

		mShader.reset(Toast::Shader::Create("../Toast/src/Toast/Renderer/ShaderTest_Vs.hlsl", "../Toast/src/Toast/Renderer/ShaderTest_ps.hlsl"));

		const std::initializer_list<Toast::BufferLayout::BufferElement>& layout = {
																   { Toast::ShaderDataType::Float3, "POSITION"},
																   { Toast::ShaderDataType::Float4, "COLOR"},
		};

		mBufferLayout.reset(Toast::BufferLayout::Create(layout, mShader));
	}

	void OnUpdate(Toast::Timestep ts) override
	{
		if (Toast::Input::IsKeyPressed(TOAST_LEFT))
			mCameraPosition.x -= mCameraMoveSpeed * ts;
		else if(Toast::Input::IsKeyPressed(TOAST_RIGHT))
			mCameraPosition.x += mCameraMoveSpeed * ts;

		if(Toast::Input::IsKeyPressed(TOAST_UP))
			mCameraPosition.y += mCameraMoveSpeed * ts;
		else if(Toast::Input::IsKeyPressed(TOAST_DOWN))
			mCameraPosition.y -= mCameraMoveSpeed * ts;

		if (Toast::Input::IsKeyPressed(TOAST_A))
			mCameraRotation += mCameraRotationSpeed * ts;
		else if (Toast::Input::IsKeyPressed(TOAST_D))
			mCameraRotation -= mCameraRotationSpeed * ts;

		const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

		Toast::RenderCommand::SetRenderTargets();
		Toast::RenderCommand::Clear(clearColor);

		mCamera.SetPosition(mCameraPosition);
		mCamera.SetRotation(mCameraRotation);

		Toast::Renderer::BeginScene(mCamera);

		DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f);

		for (int y = 0; y < 10; y++)
		{
			for (int x = 0; x < 10; x++)
			{
				DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * scale;
				transform = transform * DirectX::XMMatrixTranslation(x * 0.11f, y * 0.11f, 0.0f);

				Toast::Renderer::Submit(mIndexBuffer, mShader, mBufferLayout, mVertexBuffer, transform);
			}
		}

		Toast::Renderer::EndScene();
	}

	virtual void OnImGuiRender() override
	{
	}

	void OnEvent(Toast::Event& event) override
	{
	}
private:
	std::shared_ptr<Toast::Shader> mShader;
	std::shared_ptr<Toast::BufferLayout> mBufferLayout;
	std::shared_ptr<Toast::VertexBuffer> mVertexBuffer;
	std::shared_ptr<Toast::IndexBuffer> mIndexBuffer;

	Toast::OrthographicCamera mCamera;
	DirectX::XMFLOAT3 mCameraPosition;
	float mCameraMoveSpeed = 5.0f;
	float mCameraRotation = 0.0f;
	float mCameraRotationSpeed = 180.0f;
};

class Mars : public Toast::Application 
{
public:
	Mars()
	{
		PushLayer(new ExampleLayer());
	}

	~Mars()
	{
	}
};

Toast::Application* Toast::CreateApplication() 
{
	return new Mars();
}