#include "tpch.h"
#include "Application.h"

#include "Toast/Log.h"

#include "Toast/Input.h"

namespace Toast {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::sInstance = nullptr;

	Application::Application()
	{
		TOAST_CORE_ASSERT(!sInstance, "Application already exists");

		sInstance = this;

		mWindow = std::unique_ptr<Window>(Window::Create());
		mWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));

		mImGuiLayer = new ImGuiLayer();
		PushOverlay(mImGuiLayer);

		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
			0.0f, 0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f,
			0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f
		};

		mVertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices), uint32_t(3)));

		uint32_t indices[3] = { 0, 1, 2 };

		mIndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));

		mShader.reset(Shader::Create("../Toast/src/Toast/Renderer/ShaderTest_Vs.hlsl", "../Toast/src/Toast/Renderer/ShaderTest_ps.hlsl"));

		const std::initializer_list<BufferLayout::BufferElement>& layout = {
																   { ShaderDataType::Float3, "POSITION"},
																   { ShaderDataType::Float4, "COLOR"},
		};

		mBufferLayout.reset(BufferLayout::Create(layout, mShader));
	}

	void Application::PushLayer(Layer* layer) 
	{
		mLayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		mLayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));

		for (auto it = mLayerStack.end(); it != mLayerStack.begin(); ) 
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run() 
	{
		while (mRunning) 
		{
			const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
			
			RenderCommand::SetRenderTargets();
			RenderCommand::Clear(clearColor);

			Renderer::BeginScene();

			mBufferLayout->Bind();
			mVertexBuffer->Bind();
			mIndexBuffer->Bind();
			mShader->Bind();
			Renderer::Submit(mIndexBuffer);

			Renderer::EndScene();

			for (Layer* layer : mLayerStack) 
				layer->OnUpdate();

			mImGuiLayer->Begin();
			for (Layer* layer : mLayerStack) 
				layer->OnImGuiRender();
			mImGuiLayer->End();

			mWindow->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		mRunning = false;

		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		mWindow->Resize((UINT)(e.GetWidth()), (UINT)(e.GetHeight()));

		return true;
	}
}