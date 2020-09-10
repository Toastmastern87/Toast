#pragma once

#include <DirectXMath.h>

namespace Toast {
	
	struct TransformComponent 
	{
		DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const DirectX::XMMATRIX& transform)
			: Transform(transform) {}

		operator DirectX::XMMATRIX& () { return Transform; }
		operator const DirectX::XMMATRIX& () const { return Transform; }
	};

	struct SpriteRendererComponent
	{
		DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const DirectX::XMFLOAT4& color)
			: Color(color) {}
	};
}