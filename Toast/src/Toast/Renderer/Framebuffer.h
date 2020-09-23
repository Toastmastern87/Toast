#pragma once

#include <DirectXMath.h>

#include "Toast/Core/Base.h"
#include "Toast/Renderer/Formats.h"

namespace Toast {

	struct FramebufferSpecification 	
	{
		uint32_t Width = 0, Height = 0;

		struct BufferDesc 
		{
			FormatCode Format;
			BindFlag BindFlags;

			BufferDesc(FormatCode f, BindFlag b) : Format(f), BindFlags(b)
			{
			}

			BufferDesc() = default;
		};

		std::vector<BufferDesc> BuffersDesc;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() const = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual void Clear(const DirectX::XMFLOAT4 clearColor) = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual void* GetColorAttachmentID() const = 0;
		virtual void* GetDepthAttachmentID() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};
}