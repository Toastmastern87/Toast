#pragma once

#include <d3d11.h>

#include "Toast/Renderer/Buffer.h"

namespace Toast {

	class DirectXVertexBuffer : public VertexBuffer 
	{
	public:
		DirectXVertexBuffer(float* vertices, uint32_t size);
		virtual ~DirectXVertexBuffer();

		virtual void Bind() const;
		virtual void Unbind() const;
	private:
		ID3D11Buffer* mVertexBuffer = nullptr;
	};

	class DirectXIndexBuffer : public IndexBuffer
	{
	public:
		DirectXIndexBuffer(uint32_t* indices, uint32_t count);
		virtual ~DirectXIndexBuffer();

		virtual void Bind() const;
		virtual void Unbind() const;

		virtual uint32_t GetCount() const { return mCount; }
	private:
		ID3D11Buffer* mIndexBuffer = nullptr;
		uint32_t mCount;
	};
}