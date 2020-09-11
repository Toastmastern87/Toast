#pragma once

#include <DirectXMath.h>

namespace Toast {

	class Camera 
	{
	public:
		Camera(const DirectX::XMMATRIX& projection)
			: mProjection(projection) {}

		const DirectX::XMMATRIX& GetProjection() const { return mProjection; }
	private:
		DirectX::XMMATRIX mProjection;
	};
}