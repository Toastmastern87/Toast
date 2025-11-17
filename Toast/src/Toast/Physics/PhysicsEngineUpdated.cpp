#include "tpch.h"
#include "PhysicsEngineUpdated.h"


namespace Toast {

	PhysicsEngineUpdated::PhysicsEngineUpdated()
	{
		mScene = nullptr;
	}

	void PhysicsEngineUpdated::Initialize(Scene* scene)
	{
		mScene = scene;
	}

	void PhysicsEngineUpdated::Update(double ts)
	{

	}

	void PhysicsEngineUpdated::ApplyGravity(double ts)
	{

	}

}