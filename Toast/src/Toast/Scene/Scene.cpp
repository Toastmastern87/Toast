#include "tpch.h"
#include "Scene.h"

#include "Components.h"
#include "Toast/Renderer/Renderer2D.h"

namespace Toast {

	static void DoMath(const DirectX::XMMATRIX& transform) 
	{

	}

	static void OnTransformConstruct(entt::registry& registry, entt::entity entity) 
	{

	}

	Scene::Scene()
	{
		//entt::entity entity = mRegistry.create();
		//mRegistry.emplace<TransformComponent>(entity, DirectX::XMMatrixIdentity());

		//mRegistry.on_construct<TransformComponent>().connect<&OnTransformConstruct>();

		//if (mRegistry.has<TransformComponent>(entity))
		//	TransformComponent& transform = mRegistry.get<TransformComponent>(entity);

		//auto view = mRegistry.view<TransformComponent>();
		//for (auto entity : view) 
		//{
		//	TransformComponent& transform = view.get<TransformComponent>(entity);
		//}
		//
		//auto group = mRegistry.group<TransformComponent>(entt::get<MeshComponent>);

		//for (auto entity : group)
		//{
		//	auto&[transform, mesh] = group.get<TransformComponent, MeshComponent>(entity);
		//}
	}

	Scene::~Scene()
	{

	}

	entt::entity Scene::CreateEntity()
	{
		return mRegistry.create();
	}

	void Scene::OnUpdate(Timestep ts)
	{
		auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);

		for (auto entity : group)
		{
			auto& [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

			Renderer2D::DrawQuad(transform, sprite.Color);
		}
	}
}