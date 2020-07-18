#include "tpch.h"
#include "Application.h"

#include "Toast/Log.h"

#include "Toast/Input.h"

namespace Toast {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::sInstance = nullptr;
	
	static DXGI_FORMAT ShaderDataTypeToDirectXBaseType(ShaderDataType type)
	{
		switch (type) 
		{
			case Toast::ShaderDataType::Float:		return DXGI_FORMAT_R32_FLOAT;
			case Toast::ShaderDataType::Float2:		return DXGI_FORMAT_R32G32_FLOAT;
			case Toast::ShaderDataType::Float3:		return DXGI_FORMAT_R32G32B32_FLOAT;
			case Toast::ShaderDataType::Float4:		return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case Toast::ShaderDataType::Int:		return DXGI_FORMAT_R32_UINT;
			case Toast::ShaderDataType::Int2:		return DXGI_FORMAT_R32G32_UINT;
			case Toast::ShaderDataType::Int3:		return DXGI_FORMAT_R32G32B32_UINT;
			case Toast::ShaderDataType::Int4:		return DXGI_FORMAT_R32G32B32A32_UINT;
		}

		TOAST_CORE_ASSERT(false, "Unkown ShaderDataType!");
		return DXGI_FORMAT_UNKNOWN;
	}

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

		mVertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));

		{
			BufferLayout layout = {
				{ ShaderDataType::Float3, "POSITION"},
				{ ShaderDataType::Float4, "COLOR"},
			};

			mVertexBuffer->SetLayout(layout);
		}

		const auto& layout = mVertexBuffer->GetLayout();

		uint32_t index = 0;
		D3D11_INPUT_ELEMENT_DESC* inputLayoutDesc = new D3D11_INPUT_ELEMENT_DESC[layout.GetElements().size()];

		for (const auto& element : layout)
		{
			inputLayoutDesc[index].SemanticName = element.mName.c_str();
			inputLayoutDesc[index].SemanticIndex = element.mSemanticIndex;
			inputLayoutDesc[index].Format = ShaderDataTypeToDirectXBaseType(element.mType);
			inputLayoutDesc[index].InputSlot = 0;
			inputLayoutDesc[index].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			inputLayoutDesc[index].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			inputLayoutDesc[index].InstanceDataStepRate = 0;

			index++;
		}

		uint32_t indices[3] = { 0, 1, 2 };

		mIndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));

		mShader.reset(new Shader("../Toast/src/Toast/Renderer/ShaderTest_Vs.hlsl", "../Toast/src/Toast/Renderer/ShaderTest_ps.hlsl"));

		ID3D10Blob* VSRaw = mShader->GetVSRaw();

		mWindow->GetGraphicsContext()->GetD3D11Device()->CreateInputLayout(inputLayoutDesc, 
																		   2, 
																		   VSRaw->GetBufferPointer(), 
																		   VSRaw->GetBufferSize(), 
																		   &mInputLayout);

		delete[] inputLayoutDesc;
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
			mWindow->Start();

			mWindow->GetGraphicsContext()->GetD3D11DeviceContext()->IASetInputLayout(mInputLayout);
			mVertexBuffer->Bind();
			mIndexBuffer->Bind();
			mShader->Bind();

			mWindow->GetGraphicsContext()->GetD3D11DeviceContext()->DrawIndexed(mIndexBuffer->GetCount(), 0, 0);

			for (Layer* layer : mLayerStack) 
				layer->OnUpdate();

			mImGuiLayer->Begin();
			for (Layer* layer : mLayerStack) 
				layer->OnImGuiRender();
			mImGuiLayer->End();

			mWindow->End();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		mRunning = false;

		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		mWindow->OnResize((UINT)(e.GetWidth()), (UINT)(e.GetHeight()));

		return true;
	}
}