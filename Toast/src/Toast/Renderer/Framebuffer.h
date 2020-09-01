#pragma once

#include "Toast/Core/Base.h"

namespace Toast {

	struct FramebufferSpecification 	
	{
		uint32_t Width, Height;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class Framebuffer
	{
	public:
		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual void* ColorAttachmentID() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);

		virtual void Clear(const float clearColor[4]) = 0;
	};
}