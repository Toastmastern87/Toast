#include "tpch.h"
#include "MovementSystem.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"

#include "Toast/Physics/PhysicsEngine.h" 

namespace Toast {

	// Snap a position to the terrain surface along its radial direction.
	void MovementSystem::SnapToSurface(Vector3& pos, Vector3& outNormal, float groundOffset)
	{
		PhysicsEngine* physics = mScene->GetPhysicsEngine().get();

		double radialDist;
		Vector3 groundNormal;
		double altitude = physics->GetAltitudeAtWorldPos(pos, radialDist, groundNormal);

		//TOAST_CORE_INFO("Snap: in=(%.2lf,%.2lf,%.2lf) altitude=%.2f normal=(%.2lf,%.2lf,%.2lf) out=(%.2lf,%.2lf,%.2lf)",
		//	pos.x, pos.y, pos.z, altitude,
		//	groundNormal.x, groundNormal.y, groundNormal.z,
		//	pos.x, pos.y, pos.z);

		outNormal = groundNormal;
		pos = pos - groundNormal * altitude;
		pos = pos + groundNormal * (double)groundOffset;
	}

	void MovementSystem::OrientToSurface(TransformComponent& tc, const Vector3& up, const Vector3& forward)
	{
		using namespace DirectX;

		Vector3 u = Vector3::Normalize(up);
		Vector3 f = Vector3::Normalize(forward - u * Vector3::Dot(forward, u));

		XMVECTOR localX = XMVectorSet(1, 0, 0, 0);
		XMVECTOR target = XMVectorSet((float)f.x, (float)f.y, (float)f.z, 0);
		float dotXF = XMVectorGetX(XMVector3Dot(localX, target));

		XMVECTOR q1;
		if (dotXF > 0.9999f) {
			q1 = XMQuaternionIdentity();
		}
		else if (dotXF < -0.9999f) {
			q1 = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XM_PI);
		}
		else {
			XMVECTOR axis = XMVector3Normalize(XMVector3Cross(localX, target));
			float angle = acosf(dotXF);
			q1 = XMQuaternionRotationAxis(axis, angle);
		}

		XMVECTOR localY = XMVectorSet(0, 1, 0, 0);
		XMVECTOR localYAfterQ1 = XMVector3Rotate(localY, q1);
		XMVECTOR targetU = XMVectorSet((float)u.x, (float)u.y, (float)u.z, 0);

		// Project both onto plane perpendicular to target (f).
		// localYProj = localYAfterQ1 - target * dot(localYAfterQ1, target)
		XMVECTOR dotYAfter_f = XMVector3Dot(localYAfterQ1, target);   // broadcast scalar
		XMVECTOR localYProj = XMVector3Normalize(XMVectorSubtract(localYAfterQ1, XMVectorMultiply(target, dotYAfter_f)));

		XMVECTOR dotU_f = XMVector3Dot(targetU, target);
		XMVECTOR uProj = XMVector3Normalize(XMVectorSubtract(targetU, XMVectorMultiply(target, dotU_f)));

		float dotYU = XMVectorGetX(XMVector3Dot(localYProj, uProj));
		dotYU = fmaxf(-1.0f, fminf(1.0f, dotYU));
		float angle2 = acosf(dotYU);

		XMVECTOR crossYU = XMVector3Cross(localYProj, uProj);
		if (XMVectorGetX(XMVector3Dot(crossYU, target)) < 0) angle2 = -angle2;

		XMVECTOR q2 = XMQuaternionRotationAxis(target, angle2);
		XMVECTOR desired = XMQuaternionMultiply(q1, q2);

		XMStoreFloat4(&tc.RotationQuaternion, XMQuaternionNormalize(desired));
		tc.RotationEulerAngles = { 0.0f, 0.0f, 0.0f };
		tc.IsDirty = true;
	}

	void MovementSystem::OnUpdate(Timestep ts)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		auto view = mScene->GetRegistry().view<TransformComponent, MoveCommandComponent, MoveableComponent>();
		for (auto entity : view)
		{
			auto& tc = view.get<TransformComponent>(entity);
			auto& cmd = view.get<MoveCommandComponent>(entity);
			auto& cfg = view.get<MoveableComponent>(entity);

			if (!cfg.IsActive) {
				mScene->GetRegistry().remove<MoveCommandComponent>(entity);
				continue;
			}
			cmd.MarkerElapsed += (float)ts;

			Vector3 currentPos = Vector3(tc.Translation.x, tc.Translation.y, tc.Translation.z);  // rendered-frame
			Vector3 toTarget = cmd.TargetWorldPos - currentPos;
			double step = (double)cmd.Speed * (double)ts;

			// Compute tangent-projected direction & distance — this is the actual movement axis.
			Vector3 normalHere;
			{
				double rd; Vector3 gn;
				mScene->GetPhysicsEngine()->GetAltitudeAtWorldPos(currentPos + worldTranslation, rd, gn);
				normalHere = gn;
			}
			Vector3 tangentVec = toTarget - normalHere * Vector3::Dot(toTarget, normalHere);
			double tangentRemaining = tangentVec.Length();
			Vector3 tangentDir = (tangentRemaining > 1e-6) ? (tangentVec / tangentRemaining) : Vector3();

			bool arriving = (step >= tangentRemaining);
			Vector3 candidatePos = arriving ? cmd.TargetWorldPos : (currentPos + tangentDir * step);

			// Snap the candidate (rendered-frame) onto terrain. SnapToSurface needs absolute input,
			// operates in absolute, then we convert back to rendered-frame.
			Vector3 candidateAbs = candidatePos + worldTranslation;
			Vector3 surfaceNormal;
			SnapToSurface(candidateAbs, surfaceNormal, cfg.GroundOffset);   // mutates candidateAbs to snapped
			Vector3 snappedRendered = candidateAbs - worldTranslation;

			// Orientation: forward = travel direction (already correct in rendered-frame; tangent for in-flight,
			// toTarget for arrival)
			Vector3 facingDir = arriving ? (tangentRemaining > 1e-6 ? tangentDir : Vector3(1.0, 0.0, 0.0)) : (snappedRendered - currentPos);
			OrientToSurface(tc, surfaceNormal, facingDir);

			// Write back position
			tc.Translation = { (float)snappedRendered.x, (float)snappedRendered.y, (float)snappedRendered.z };
			tc.IsDirty = true;

			if (arriving)
				mScene->GetRegistry().remove<MoveCommandComponent>(entity);
		}
	}

}