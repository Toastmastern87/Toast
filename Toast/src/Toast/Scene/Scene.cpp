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

	void Scene::DestroyEntity(Entity entity)
	{
		mRegistry.destroy(entity);
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

		// TODO, should this really be here since they only need to combined ones and not every update cycle
		// Combines a planet with its mesh
		auto view = mRegistry.view<PlanetComponent, PrimitiveMeshComponent>();
		for (auto entity : view)
		{
			auto [planet, mesh] = view.get<PlanetComponent, PrimitiveMeshComponent>(entity);

			planet.Planet->SetMesh(mesh.Mesh);
		}

		SceneCamera* mainCamera = nullptr;
		DirectX::XMMATRIX cameraTransform;
		{
			auto view = mRegistry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = transform.GetTransform();
					break;
				}
			}
		}

		if (mainCamera)
		{
			// 3D Rendering
			Renderer::BeginScene(*mainCamera, cameraTransform);
			{
				auto view = mRegistry.view<TransformComponent, PrimitiveMeshComponent>();

				for (auto entity : view)
				{
					auto [transform, mesh] = view.get<TransformComponent, PrimitiveMeshComponent>(entity);

					// TODO rename IsMeshActive to IsValid() 
					if (mesh.Mesh->IsMeshActive()) {
						switch (mSettings.WireframeRendering) 
						{
						case Settings::Wireframe::NO:
						{
							Renderer::SubmitMesh(mesh.Mesh, transform.GetTransform(), false);

							break;
						}
						case Settings::Wireframe::YES:
						{
							Renderer::SubmitMesh(mesh.Mesh, transform.GetTransform(), true);

							break;
						}
						case Settings::Wireframe::ONTOP:
						{
							// TODO, need a way to render a mesh total white

							break;
						}
						}

					}
				}
			}
			
			// Draw grid
			// TODO - Gradient, aka 150.0f should come from some kind of option instead
			if(mSettings.GridActivated)
				Renderer::SubmitGrid(*mainCamera, cameraTransform, { mainCamera->GetPerspectiveFarClip(), mainCamera->GetPerspectiveNearClip(), 150.0f });

			Renderer::EndScene();

			// 2D Rendering
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			{
				auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);

				for (auto entity : group)
				{
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

					Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
				}
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

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component) 
	{
		static_assert(false);
	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<PrimitiveMeshComponent>(Entity entity, PrimitiveMeshComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<PlanetComponent>(Entity entity, PlanetComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		component.Camera.SetViewportSize(mViewportWidth, mViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component)
	{
	}

}