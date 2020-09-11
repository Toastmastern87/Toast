#pragma once

#include <DirectXMath.h>

#include "Toast/Renderer/Camera.h"

namespace Toast {
	
	struct TagComponent 
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {}
	};

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

	struct CameraComponent 
	{
		Toast::Camera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
		CameraComponent(const DirectX::XMMATRIX& projection)
			: Camera(projection) {}
	};
}