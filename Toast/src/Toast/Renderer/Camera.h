#pragma once

#include <DirectXMath.h>

namespace Toast {

	class Camera
	{
	public:
		Camera() = default;
		Camera(const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& invViewMatrix, const DirectX::XMFLOAT4X4& projection, const DirectX::XMFLOAT4X4& invProjection, float farClip, float nearClip)
			: mViewMatrix(viewMatrix), mInvViewMatrix(invViewMatrix), mProjection(projection), mInvProjection(invProjection), mFarClip(farClip), mNearClip(nearClip) {}

		const DirectX::XMFLOAT4X4& GetViewMatrix() const { return mViewMatrix; }
		const DirectX::XMFLOAT4X4& GetInvViewMatrix() const { return mInvViewMatrix; }
		void SetViewMatrix(DirectX::XMFLOAT4X4 viewMatrix) { mViewMatrix = viewMatrix; }
		void SetInvViewMatrix(DirectX::XMFLOAT4X4 invViewMatrix) { mInvViewMatrix = invViewMatrix; }

		const DirectX::XMFLOAT4X4& GetProjection() const { return mProjection; }
		const DirectX::XMFLOAT4X4& GetInvProjection() const { return mInvProjection; }

		virtual float& GetNearClip() { return mNearClip; }
		virtual float& GetFarClip() { return mFarClip; }
		
		virtual DirectX::XMFLOAT4& GetForwardDirection() = 0;
	protected:
		DirectX::XMFLOAT4X4 mViewMatrix, mInvViewMatrix, mProjection, mInvProjection;
		float mFarClip = 1000.0f, mNearClip = 0.1f;
	};
}