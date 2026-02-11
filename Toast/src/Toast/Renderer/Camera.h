#pragma once

#include <DirectXMath.h>

#include "Toast/Core/Log.h"

#include "Toast/Core/Math/Vector.h"
#include "Toast/Core/Math/Quaternion.h"

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

		DirectX::XMFLOAT3& GetTranslation() { return mTranslation; }
		void SetTranslation(DirectX::XMFLOAT3 translation) { mTranslation = translation; }

		void AddWorldTranslation(DirectX::XMFLOAT3 worldTranslation) 
		{
			mWorldTranslation = { mWorldTranslation.x + worldTranslation.x, mWorldTranslation.y + worldTranslation.y , mWorldTranslation.z + worldTranslation.z };
		}
		const DirectX::XMMATRIX& GetWorldTranslationMatrix() const { return DirectX::XMMatrixIdentity() * DirectX::XMMatrixTranslation(mWorldTranslation.x, mWorldTranslation.y, mWorldTranslation.z); }
		DirectX::XMFLOAT3& GetWorldTranslation() { return mWorldTranslation; }

		const DirectX::XMFLOAT4X4& GetProjection() const { return mProjection; }
		const DirectX::XMFLOAT4X4& GetInvProjection() const { return mInvProjection; }

		const DirectX::XMFLOAT4X4& GetOrthoProjection() const { return mOrthoProjection; }
		const DirectX::XMFLOAT4X4& GetInvOrthoProjection() const { return mInvOrthoProjection; }

		virtual float& GetNearClip() { return mNearClip; }
		virtual float& GetFarClip() { return mFarClip; }

		const float GetVerticalFOV() const { return DirectX::XMConvertToDegrees(mFOV); }
		const float GetAspectRatio() const { return mAspectRatio; }
		
		// OLD
		virtual DirectX::XMFLOAT4& GetForwardDirection() = 0;

		// NEW
		virtual Vector3 GetForwardVectorWS(Quaternion cameraRot) = 0;
		virtual Vector3 GetUpVectorWS(Quaternion cameraRot) = 0;
		virtual Vector3 GetRightVectorWS(Quaternion cameraRot) = 0;
	protected:
		DirectX::XMFLOAT4X4 mViewMatrix, mInvViewMatrix, mProjection, mInvProjection, mOrthoProjection, mInvOrthoProjection;

		DirectX::XMFLOAT3 mTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mWorldTranslation = { 0.0f, 0.0f, 0.0f };

		float mFOV = DirectX::XMConvertToRadians(45.0f);
		float mAspectRatio = 1.778f;

		float mFarClip = 1000.0f, mNearClip = 0.1f;
	};
}