#pragma once

namespace Toast {

	class Scene;

	class PhysicsEngineUpdated
	{
	public:
		PhysicsEngineUpdated();

		void Initialize(Scene* scene);
		void Update(double ts);
	private:
		void ApplyGravity(double ts);
	private:
		Scene* mScene;
	};
}