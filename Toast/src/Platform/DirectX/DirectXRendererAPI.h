#pragma once

#include <d3d11.h>

#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	class DirectXRendererAPI : public RendererAPI
	{
	public:
		virtual void Clear(const float clearColor[4]) override;
		virtual void SetRenderTargets() override;
		virtual void DrawIndexed(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

	};
}