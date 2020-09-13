#pragma once

#include <DirectXMath.h>

namespace Toast {

	class Camera 
	{
	public:
		Camera() = default;
		Camera(const DirectX::XMMATRIX& projection)
			: mProjection(projection) {}

		const DirectX::XMMATRIX& GetProjection() const { return mProjection; }
	protected:
		DirectX::XMMATRIX mProjection = DirectX::XMMatrixIdentity();
	};
}