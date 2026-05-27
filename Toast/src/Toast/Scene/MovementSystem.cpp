#include "tpch.h"
#include "MovementSystem.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"

#include "Toast/Physics/PhysicsEngine.h" 

namespace Toast {

	// Snap a position to the terrain surface along its radial direction.
	void MovementSystem::SnapToSurface(Vector3& pos, Vector3& outNormal)
	{
		PhysicsEngine* physics = mScene->GetPhysicsEngine().get();

		double radialDist;
		Vector3 groundNormal;
		double altitude = physics->GetAltitudeAtWorldPos(pos, radialDist, groundNormal);

		pos = pos - groundNormal * altitude;
	}

	// Orient so 'up' = surface normal, 'forward' = travel direction (gram-Schmidt).
	void MovementSystem::OrientToSurface(TransformComponent& tc, const Vector3& up, const Vector3& forward)
	{
		Vector3 u = Vector3::Normalize(up);
		Vector3 f = Vector3::Normalize(forward - u * Vector3::Dot(forward, u));
		Vector3 r = Vector3::Normalize(Vector3::Cross(u, f));  
		f = Vector3::Cross(r, u);   
		DirectX::XMMATRIX rot = DirectX::XMMatrixSet(
			(float)r.x, (float)r.y, (float)r.z, 0.0f,
			(float)u.x, (float)u.y, (float)u.z, 0.0f,
			(float)f.x, (float)f.y, (float)f.z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);
		DirectX::XMVECTOR desired = DirectX::XMQuaternionRotationMatrix(rot);

		DirectX::XMVECTOR qEuler = DirectX::XMQuaternionRotationRollPitchYaw(
			DirectX::XMConvertToRadians(tc.RotationEulerAngles.x),
			DirectX::XMConvertToRadians(tc.RotationEulerAngles.y),
			DirectX::XMConvertToRadians(tc.RotationEulerAngles.z));

		DirectX::XMVECTOR stored = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(qEuler), desired);
		DirectX::XMStoreFloat4(&tc.RotationQuaternion, DirectX::XMQuaternionNormalize(stored));
		tc.IsDirty = true;
	}

	void MovementSystem::OnUpdate(Timestep ts)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		Vector3 planetCenter = mScene->GetPlanet()->GetTranslation(); 
		double planetRadius = mScene->GetPlanet()->GetRadius(); 

		auto view = mScene->GetRegistry().view<TransformComponent, MoveCommandComponent, MoveableComponent>();
		for (auto entity : view)
		{
			auto& tc = view.get<TransformComponent>(entity);
			auto& cmd = view.get<MoveCommandComponent>(entity);
			auto& cfg = view.get<MoveableComponent>(entity);

			// Deactivation cancels in-flight commands immediately
			if (!cfg.IsActive)
			{
				mScene->GetRegistry().remove<MoveCommandComponent>(entity);
				continue;
			}

			cmd.MarkerElapsed += (float)ts;   // marker timer; renderer reads this

			Vector3 currentPos = Vector3(tc.Translation.x, tc.Translation.y, tc.Translation.z) + worldTranslation;
			Vector3 toTarget = cmd.TargetWorldPos - currentPos;
			double remaining = toTarget.Length();
			double step = (double)cmd.Speed * (double)ts;

			Vector3 newWorldPos;
			Vector3 surfaceNormal;

			if (step >= remaining)
			{
				newWorldPos = cmd.TargetWorldPos;
				SnapToSurface(newWorldPos, surfaceNormal);
				OrientToSurface(tc, surfaceNormal, toTarget);
				mScene->GetRegistry().remove<MoveCommandComponent>(entity);
				continue;
			}
			else
			{
				Vector3 normalHere;
				{
					double rd; Vector3 gn;
					mScene->GetPhysicsEngine()->GetAltitudeAtWorldPos(currentPos, rd, gn);
					normalHere = gn;
				}
				Vector3 tangentDir = Vector3::Normalize(toTarget - normalHere * Vector3::Dot(toTarget, normalHere));

				newWorldPos = currentPos + tangentDir * step;
				SnapToSurface(newWorldPos, surfaceNormal);
				OrientToSurface(tc, surfaceNormal, tangentDir);
			}

			Vector3 localPos = newWorldPos - worldTranslation;
			tc.Translation = { (float)localPos.x, (float)localPos.y, (float)localPos.z };
			tc.IsDirty = true;
		}
	}

}