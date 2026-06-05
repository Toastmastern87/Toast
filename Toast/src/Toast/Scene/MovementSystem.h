#pragma once
#include "Toast/Scene/Components.h"
#include "Toast/Core/Math/Vector.h"
#include "Toast/Core/Timestep.h"   

namespace Toast {

	class Scene;

	class MovementSystem
	{
	public:
		MovementSystem(Scene* scene) : mScene(scene) {}

		void OnUpdate(Timestep ts);
	private:
		void SnapToSurface(Vector3& pos, Vector3& outNormal, float groundOffset);
		void OrientToSurface(TransformComponent& tc, const Vector3& up, const Vector3& forward);
	private:
		Scene* mScene;

		Vector3 mLastCamWorld;
	};

}