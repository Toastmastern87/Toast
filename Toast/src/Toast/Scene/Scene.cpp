#include "tpch.h"
#include "Scene.h"

#include "Components.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Entity.h"

namespace Toast {

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{

	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { mRegistry.create(), this };
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		return entity;
	}

	void Scene::OnUpdate(Timestep ts)
	{
		//Update scripts
		{
			mRegistry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
			{
				if (!nsc.Instance) 
				{
					nsc.Instance = nsc.InstantiateScript();
					nsc.Instance->mEntity = Entity{ entity, this };
					nsc.Instance->OnCreate();
				}

				nsc.Instance->OnUpdate(ts);
			});
		}

		Camera* mainCamera = nullptr;
		DirectX::XMMATRIX* cameraTransform = nullptr;
		{
			auto view = mRegistry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = &transform.Transform;
					break;
				}
			}
		}

		if (mainCamera) 
		{
			// 3D Rendering
			Renderer::BeginScene(mainCamera->GetProjection(), *cameraTransform);

			// Draw grid
			Renderer::SubmitQuad(DirectX::XMMatrixIdentity());

			Renderer::EndScene();

			// 2D Rendering
			Renderer2D::BeginScene(mainCamera->GetProjection(), *cameraTransform);

			auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);

			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

				Renderer2D::DrawQuad(transform, sprite.Color);
			}

			Renderer2D::EndScene();
		}
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		mViewportWidth = width;
		mViewportHeight = height;

		auto view = mRegistry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
			{
				cameraComponent.Camera.SetViewportSize(width, height);
			}
		}
	}

}