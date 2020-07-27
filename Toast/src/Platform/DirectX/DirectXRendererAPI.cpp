#include "tpch.h"
#include "DirectXRendererAPI.h"

#include "DirectXContext.h"
#include "Toast/Application.h"

#include <d3d11.h>

namespace Toast {

	void DirectXRendererAPI::Clear(const float clearColor[4])
	{
		Application& app = Application::Get();
		ID3D11RenderTargetView* renderTargetView = app.GetWindow().GetContext()->GetRenderTargetView();
		ID3D11DeviceContext*  deviceContext = app.GetWindow().GetContext()->GetDeviceContext();

		deviceContext->ClearRenderTargetView(renderTargetView, clearColor);
	}

	void DirectXRendererAPI::SetRenderTargets() 
	{
		Application& app = Application::Get();
		ID3D11RenderTargetView* renderTargetView = app.GetWindow().GetContext()->GetRenderTargetView();
		ID3D11DeviceContext* deviceContext = app.GetWindow().GetContext()->GetDeviceContext();

		deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
	}

	void DirectXRendererAPI::DrawIndexed(const Ref<IndexBuffer>& indexBuffer)
	{
		Application& app = Application::Get();
		ID3D11DeviceContext* deviceContext = app.GetWindow().GetContext()->GetDeviceContext();

		deviceContext->DrawIndexed(indexBuffer->GetCount(), 0, 0);
	}
}