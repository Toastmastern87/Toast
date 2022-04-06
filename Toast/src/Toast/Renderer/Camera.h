#pragma once

#include <DirectXMath.h>

namespace Toast {

	class Camera 
	{
	public:
		Camera() = default;
		Camera(const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& invViewMatrix, const DirectX::XMFLOAT4X4& projection, const DirectX::XMFLOAT4X4& invProjection)
			: mViewMatrix(viewMatrix), mInvViewMatrix(invViewMatrix), mProjection(projection), mInvProjection(invProjection) {}

		const DirectX::XMFLOAT4X4& GetViewMatrix() const { return mViewMatrix; }
		const DirectX::XMFLOAT4X4& GetInvViewMatrix() const { return mInvViewMatrix; }
		void SetViewMatrix(DirectX::XMFLOAT4X4 viewMatrix) {  mViewMatrix = viewMatrix; }
		void SetInvViewMatrix(DirectX::XMFLOAT4X4 invViewMatrix) { mInvViewMatrix = invViewMatrix; }

		const DirectX::XMFLOAT4X4& GetProjection() const { return mProjection; }
		const DirectX::XMFLOAT4X4& GetInvProjection() const { return mInvProjection; }
	protected:
		DirectX::XMFLOAT4X4 mViewMatrix, mInvViewMatrix, mProjection, mInvProjection;
	};
}