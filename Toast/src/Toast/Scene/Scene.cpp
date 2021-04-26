#include "tpch.h"
#include "Scene.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"
#include "Toast/Renderer/Primitives.h"

#include "Toast/Script/ScriptEngine.h"

namespace Toast {

	std::unordered_map<UUID, Scene*> sActiveScenes;

	struct SceneComponent
	{
		UUID SceneID;
	};

	static void OnScriptComponentConstruct(entt::registry& registry, entt::entity entity)
	{
		auto sceneView = registry.view<SceneComponent>();
		UUID sceneID = registry.get<SceneComponent>(sceneView.front()).SceneID;

		Scene* scene = sActiveScenes[sceneID];

		auto entityID = registry.get<IDComponent>(entity).ID;
		TOAST_CORE_ASSERT(scene->mEntityIDMap.find(entityID) != scene->mEntityIDMap.end(), "");
		ScriptEngine::InitScriptEntity(scene->mEntityIDMap.at(entityID));
	}

	static void OnScriptComponentDestroy(entt::registry& registry, entt::entity entity)
	{
		// TO DO!
	}

	Scene::Scene()
	{
		mRegistry.on_construct<ScriptComponent>().connect<&OnScriptComponentConstruct>();
		mRegistry.on_destroy<ScriptComponent>().connect<&OnScriptComponentDestroy>();

		mSceneEntity = mRegistry.create();
		mRegistry.emplace<SceneComponent>(mSceneEntity, mSceneID);

		sActiveScenes[mSceneID] = this;

		Init();
	}

	Scene::~Scene()
	{
		sActiveScenes.erase(mSceneID);
	}

	void Scene::Init()
	{
		// Initiate the skybox
		Ref<Shader> skyboxShader = CreateRef<Shader>("assets/shaders/Skybox.hlsl");
		mSkyboxMaterial = CreateRef<Material>("Skybox", skyboxShader);
		mSkybox = CreateRef<Mesh>();
		mSkybox->SetMaterial(mSkyboxMaterial);
		uint32_t indexCount = Primitives::CreateCube(mSkybox->GetVertices(), mSkybox->GetIndices());
		mSkybox->Init();
		mSkybox->AddSubmesh(indexCount);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { mRegistry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = {};

		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		mEntityIDMap[idComponent.ID] = entity;

		return entity;
	}

	Entity Scene::CreateEntityWithID(UUID uuid, const std::string& name)
	{
		Entity entity = { mRegistry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = uuid;

		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		TOAST_CORE_ASSERT(mEntityIDMap.find(uuid) == mEntityIDMap.end(), "Entity alread exist!");
		mEntityIDMap[uuid] = entity;

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		mRegistry.destroy(entity);
	}

	Toast::Entity Scene::CreateCube(const std::string& name)
	{
		Entity entity = { mRegistry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = {};

		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		auto& mesh = entity.AddComponent<MeshComponent>(CreateRef<Mesh>());
		uint32_t indexCount = Primitives::CreateCube(mesh.Mesh->GetVertices(), mesh.Mesh->GetIndices());
		mesh.Mesh->Init();
		mesh.Mesh->AddSubmesh(indexCount);

		mEntityIDMap[idComponent.ID] = entity;

		return entity;
	}

	Toast::Entity Scene::CreateSphere(const std::string& name)
	{
		Entity entity = { mRegistry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = {};

		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		auto& mesh = entity.AddComponent<MeshComponent>(CreateRef<Mesh>());
		uint32_t indexCount = Primitives::CreateSphere(mesh.Mesh->GetVertices(), mesh.Mesh->GetIndices(), 2);
		mesh.Mesh->Init();
		mesh.Mesh->AddSubmesh(indexCount);
		
		mEntityIDMap[idComponent.ID] = entity;

		return entity;
	}

	void Scene::OnRuntimeStart()
	{
		ScriptEngine::SetSceneContext(shared_from_this());

		// Update all entities with scripts
		{
			auto view = mRegistry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::InstantiateEntityClass(e);
				else
					TOAST_CORE_INFO("Module doesn't exist");
			}
		}

		mIsPlaying = true;
	}

	void Scene::OnRuntimeStop()
	{
		mIsPlaying = false;
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		DirectX::XMVECTOR cameraPos = { 0.0f, 0.0f, 0.0f }, cameraRot = { 0.0f, 0.0f, 0.0f }, cameraScale = { 0.0f, 0.0f, 0.0f };

		// Update statistics
		{
			mStats.timesteps += ts;
			if (mStats.timesteps > 0.1f)
			{
				mStats.timesteps -= 0.1f;
				mStats.FPS = 1.0f / ts;
			}

			mStats.VerticesCount = 0;
		}

		// Update all entities with scripts
		{
			auto view = mRegistry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::OnUpdateEntity(e.mScene->GetUUID(), e.GetComponent<IDComponent>().ID, ts);
				else
					TOAST_CORE_INFO("Module doesn't exist");
			}
		}

		// Process lights
		{
			mLightEnvironment = LightEnvironment();
			auto lights = mRegistry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
			uint32_t directionalLightIndex = 0;
			for (auto entity : lights)
			{
				auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);
				DirectX::XMFLOAT4 direction = { cos(transformComponent.Rotation.y) * cos(transformComponent.Rotation.x), sin(transformComponent.Rotation.y) * cos(transformComponent.Rotation.x), sin(transformComponent.Rotation.x), 0.0f, };
				DirectX::XMFLOAT4 radiance = DirectX::XMFLOAT4(lightComponent.Radiance.x, lightComponent.Radiance.y, lightComponent.Radiance.z, 0.0f);
				mLightEnvironment.DirectionalLights[directionalLightIndex++] =
				{
					direction,
					radiance,
					lightComponent.Intensity,
					lightComponent.SunDisc == true ? 1.0f : 0.0f
				};
			}
		}

		// Process Skylight
		{
			mEnvironment = Environment();
			auto skylights = mRegistry.group<SkyLightComponent>(entt::get<TransformComponent>);
			for (auto entity : skylights)
			{
				auto [transformComponent, skylightComponent] = skylights.get<TransformComponent, SkyLightComponent>(entity);
				mEnvironment = skylightComponent.SceneEnvironment;
				mEnvironmentIntensity = skylightComponent.Intensity;
				SetSkybox(mEnvironment.RadianceMap);
			}
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
				// if no camera is present nothing is rendered
				else
					return;
			}
		}

		// Checks if the game camera have moved
		if (mainCamera) {
			DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);

			if (!DirectX::XMVector3Equal(mOldCameraPos, cameraPos))
			{
				auto view = mRegistry.view<PlanetComponent, MeshComponent, TransformComponent>();
				for (auto entity : view)
				{
					auto [planet, mesh, transform] = view.get<PlanetComponent, MeshComponent, TransformComponent>(entity);

					PlanetSystem::GeneratePlanet(transform.GetTransform(), mesh.Mesh->mPlanetFaces, mesh.Mesh->mPlanetPatches, planet.MorphData.DistanceLUT, planet.FaceLevelDotLUT, cameraPos, planet.Subdivisions);

					mesh.Mesh->InitPlanet();
					mesh.Mesh->AddSubmesh((uint32_t)(mesh.Mesh->mIndices.size()));
				}
			}

			mOldCameraPos = cameraPos;
		}

		if (mainCamera)
		{
			// 3D Rendering
			Renderer::BeginScene(this, *mainCamera, DirectX::XMMatrixInverse(nullptr, cameraTransform), { 1.0f, 1.0, 1.0f, 1.0f });
			{
				{
					auto view = mRegistry.view<TransformComponent, CameraComponent>();
					for (auto entity : view)
					{
						auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
					}
				}

				// Skybox!
				{
					if (mSkyboxTexture)
						Renderer::SubmitSkybox(mSkybox, DirectX::XMFLOAT4(DirectX::XMVectorGetX(mOldCameraPos), DirectX::XMVectorGetY(mOldCameraPos), DirectX::XMVectorGetZ(mOldCameraPos), 0.0f), DirectX::XMMatrixInverse(nullptr, cameraTransform), mainCamera->GetProjection(), mEnvironmentIntensity, mSkyboxLod);
				}

				// Meshes!
				auto viewMeshes = mRegistry.view<TransformComponent, MeshComponent>();
				for (auto entity : viewMeshes)
				{
					auto [transform, mesh] = viewMeshes.get<TransformComponent, MeshComponent>(entity);

					//Do not submit mesh if it's a planet
					if (mesh.Mesh->GetMaterial()->GetName() != "Planet")
					{
						switch (mSettings.WireframeRendering)
						{
						case Settings::Wireframe::NO:
						{
							Renderer::SubmitMesh(mesh.Mesh, transform.GetTransform(), (int)entity, false);

							break;
						}
						case Settings::Wireframe::YES:
						{
							Renderer::SubmitMesh(mesh.Mesh, transform.GetTransform(), (int)entity, true);

							break;
						}
						case Settings::Wireframe::ONTOP:
						{
							// TODO

							break;
						}
						}
					}

					mStats.VerticesCount += static_cast<uint32_t>(mesh.Mesh->GetVertices().size());
				}

				// Planets!
				auto viewPlanets = mRegistry.view<TransformComponent, MeshComponent, PlanetComponent>();
				for (auto entity : viewPlanets)
				{
					auto [transform, mesh, planet] = viewPlanets.get<TransformComponent, MeshComponent, PlanetComponent>(entity);

					switch (mSettings.WireframeRendering)
					{
					case Settings::Wireframe::NO:
					{
						Renderer::SubmitPlanet(mesh.Mesh, transform.GetTransform(), (int)entity, planet.PlanetData, planet.MorphData, false);

						break;
					}
					case Settings::Wireframe::YES:
					{
						Renderer::SubmitPlanet(mesh.Mesh, transform.GetTransform(), (int)entity, planet.PlanetData, planet.MorphData, true);

						break;
					}
					case Settings::Wireframe::ONTOP:
					{
						// TODO

						break;
					}
					}

					mStats.VerticesCount += static_cast<uint32_t>(mesh.Mesh->GetPlanetVertices().size() * mesh.Mesh->GetPlanetPatches().size());
				}

				Renderer::EndScene();
			}

			// 2D Rendering
			//Renderer2D::BeginScene(*mainCamera, cameraTransform);
			//{
			//	auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);

			//	for (auto entity : group)
			//	{
			//		auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

			//		Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
			//	}
			//}

			//Renderer2D::EndScene();
		}
	}

	void Scene::OnUpdateEditor(Timestep ts, const Ref<EditorCamera> editorCamera)
	{
		DirectX::XMVECTOR cameraPos = { 0.0f, 0.0f, 0.0f }, cameraRot = { 0.0f, 0.0f, 0.0f }, cameraScale = { 0.0f, 0.0f, 0.0f };

		// Makes sure that if there is no camera the position takes the perspective editor camera position instead
		cameraPos = editorCamera->GetPosition();

		// Update statistics
		{
			mStats.timesteps += ts;
			if (mStats.timesteps > 0.1f)
			{
				mStats.timesteps -= 0.1f;
				mStats.FPS = 1.0f / ts;
			}

			mStats.VerticesCount = 0;
		}

		// Process lights
		{
			mLightEnvironment = LightEnvironment();
			auto lights = mRegistry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
			uint32_t directionalLightIndex = 0;
			for (auto entity : lights)
			{
				auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);
				DirectX::XMFLOAT4 direction = { cos(transformComponent.Rotation.y) * cos(transformComponent.Rotation.x), sin(transformComponent.Rotation.y) * cos(transformComponent.Rotation.x), sin(transformComponent.Rotation.x), 0.0f, };
				DirectX::XMFLOAT4 radiance = DirectX::XMFLOAT4(lightComponent.Radiance.x, lightComponent.Radiance.y, lightComponent.Radiance.z, 0.0f);
				mLightEnvironment.DirectionalLights[directionalLightIndex++] =
				{
					direction,
					radiance,
					lightComponent.Intensity,
					lightComponent.SunDisc == true ? 1.0f : 0.0f
				};
			}
		}

		// Process Skylight
		{
			mEnvironment = Environment();
			auto skylights = mRegistry.group<SkyLightComponent>(entt::get<TransformComponent>);
			for (auto entity : skylights)
			{
				auto [transformComponent, skylightComponent] = skylights.get<TransformComponent, SkyLightComponent>(entity);
				mEnvironment = skylightComponent.SceneEnvironment;
				mEnvironmentIntensity = skylightComponent.Intensity;
				SetSkybox(mEnvironment.RadianceMap);
			}
		}

		// Checks if the game camera have moved
		{
			auto view = mRegistry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
				if (camera.Primary)
					DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, transform.GetTransform());

			}

			if (!DirectX::XMVector3Equal(mOldCameraPos, cameraPos))
			{
				auto view = mRegistry.view<PlanetComponent, MeshComponent, TransformComponent>();
				for (auto entity : view)
				{
					auto [planet, mesh, transform] = view.get<PlanetComponent, MeshComponent, TransformComponent>(entity);

					PlanetSystem::GeneratePlanet(transform.GetTransform(), mesh.Mesh->mPlanetFaces, mesh.Mesh->mPlanetPatches, planet.MorphData.DistanceLUT, planet.FaceLevelDotLUT, cameraPos, planet.Subdivisions);

					mesh.Mesh->InitPlanet();
					mesh.Mesh->AddSubmesh((uint32_t)(mesh.Mesh->mIndices.size()));
				}
			}

			mOldCameraPos = cameraPos;
		}

		DirectX::XMFLOAT4 cameraPosFloat;
		DirectX::XMStoreFloat4(&cameraPosFloat, cameraPos);

		// 3D Rendering
		Renderer::BeginScene(this, *editorCamera, editorCamera->GetViewMatrix(), cameraPosFloat);
		{
			{
				auto view = mRegistry.view<TransformComponent, CameraComponent>();
				for (auto entity : view)
				{
					auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
				}
			}

			// Skybox!
			{
				if (mSkyboxTexture)
					Renderer::SubmitSkybox(mSkybox, DirectX::XMFLOAT4(DirectX::XMVectorGetX(editorCamera->GetPosition()), DirectX::XMVectorGetY(editorCamera->GetPosition()), DirectX::XMVectorGetZ(editorCamera->GetPosition()), 0.0f), editorCamera->GetViewMatrix(), editorCamera->GetProjection(), mEnvironmentIntensity, mSkyboxLod);
			}

			// Meshes!
			auto viewMeshes = mRegistry.view<TransformComponent, MeshComponent>();
			for (auto entity : viewMeshes)
			{
				auto [transform, mesh] = viewMeshes.get<TransformComponent, MeshComponent>(entity);

				//Do not submit mesh if it's a planet
				if (mesh.Mesh->GetMaterial()->GetName() != "Planet")
				{
					switch (mSettings.WireframeRendering)
					{
					case Settings::Wireframe::NO:
					{
						Renderer::SubmitMesh(mesh.Mesh, transform.GetTransform(), (int)entity, false);

						break;
					}
					case Settings::Wireframe::YES:
					{
						Renderer::SubmitMesh(mesh.Mesh, transform.GetTransform(), (int)entity, true);

						break;
					}
					case Settings::Wireframe::ONTOP:
					{
						// TODO

						break;
					}
					}
				}

				mStats.VerticesCount += static_cast<uint32_t>(mesh.Mesh->GetVertices().size());
			}

			// Planets!
			auto viewPlanets = mRegistry.view<TransformComponent, MeshComponent, PlanetComponent>();
			for (auto entity : viewPlanets)
			{
				auto [transform, mesh, planet] = viewPlanets.get<TransformComponent, MeshComponent, PlanetComponent>(entity);

				switch (mSettings.WireframeRendering)
				{
				case Settings::Wireframe::NO:
				{
					Renderer::SubmitPlanet(mesh.Mesh, transform.GetTransform(), (int)entity, planet.PlanetData, planet.MorphData, false);

					break;
				}
				case Settings::Wireframe::YES:
				{
					Renderer::SubmitPlanet(mesh.Mesh, transform.GetTransform(), (int)entity, planet.PlanetData, planet.MorphData, true);

					break;
				}
				case Settings::Wireframe::ONTOP:
				{
					// TODO

					break;
				}
				}

				mStats.VerticesCount += static_cast<uint32_t>(mesh.Mesh->GetPlanetVertices().size() * mesh.Mesh->GetPlanetPatches().size());
			}

			Renderer::EndScene();
		}

		//// 2D Rendering
		//Renderer2D::BeginScene(*perspectiveCamera, perspectiveCamera->GetViewMatrix());
		//{
		//	auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);

		//	for (auto entity : group)
		//	{
		//		auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

		//		Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
		//	}
		//}
		//Renderer2D::EndScene();

		// Debug Rendering
		RendererDebug::BeginScene(*editorCamera);
		{
			RenderCommand::SetPrimitiveTopology(Topology::LINELIST);

			auto view = mRegistry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				RendererDebug::SubmitCameraFrustum(camera.Camera, transform.GetTransform(), transform.Translation);
			}
		}
		RendererDebug::EndScene();
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

	void Scene::SetSkybox(const Ref<TextureCube>& skybox)
	{
		mSkyboxTexture = skybox;
		mSkyboxMaterial->SetTexture(3, D3D11_PIXEL_SHADER, mSkyboxTexture);
	}

	Toast::Entity Scene::FindEntityByTag(const std::string& tag)
	{
		auto view = mRegistry.view<TagComponent>();
		for (auto entity : view)
		{
			const auto& canditate = view.get<TagComponent>(entity).Tag;
			if (canditate == tag)
				return Entity(entity, this);
		}

		return Entity{};
	}

	template<typename T>
	static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		auto components = srcRegistry.view<T>();
		for (auto srcEntity : components)
		{
			entt::entity destEntity = enttMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);

			auto& srcComponent = srcRegistry.get<T>(srcEntity);
			auto& destComponent = dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
		}
	}

	void Scene::CopyTo(Ref<Scene>& target)
	{
		// Environment
		target->mLightEnvironment = mLightEnvironment;

		target->mEnvironment = mEnvironment;
		target->mSkyboxTexture = mSkyboxTexture;
		target->mSkyboxMaterial = mSkyboxMaterial;
		target->mSkyboxLod = mSkyboxLod;

		std::unordered_map<UUID, entt::entity> enttMap;
		auto idComponent = mRegistry.view<IDComponent>();
		for (auto entity : idComponent)
		{
			auto uuid = mRegistry.get<IDComponent>(entity).ID;
			Entity e = target->CreateEntityWithID(uuid, "");
			enttMap[uuid] = e.mEntityHandle;
		}

		CopyComponent<TagComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<TransformComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<MeshComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<PlanetComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<CameraComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<DirectionalLightComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SkyLightComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<ScriptComponent>(target->mRegistry, mRegistry, enttMap);

		const auto& entityInstanceMap = ScriptEngine::GetEntityInstanceMap();
		if (entityInstanceMap.find(target->GetUUID()) != entityInstanceMap.end())
			ScriptEngine::CopyEntityScriptData(target->GetUUID(), mSceneID);
		else
			TOAST_CORE_WARN("NO Data being copied");
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component) 
	{
		static_assert(false);
	}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component)
	{
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
	void Scene::OnComponentAdded<MeshComponent>(Entity entity, MeshComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<PlanetComponent>(Entity entity, PlanetComponent& component)
	{
		TransformComponent tc;
		MeshComponent mc;

		if (!entity.HasComponent<MeshComponent>()) 
			mc = entity.AddComponent<MeshComponent>(CreateRef<Mesh>());
		else 
			mc = entity.GetComponent<MeshComponent>();

		mc.Mesh->SetMaterial(MaterialLibrary::Get("Planet"));
		mc.Mesh->mTopology = PrimitiveTopology::TRIANGLELIST;

		SceneCamera* mainCamera = nullptr;
		DirectX::XMMATRIX cameraTransform;
		auto view = mRegistry.view<TransformComponent, CameraComponent>();
		for (auto cameraEntity : view)
		{
			auto [transform, camera] = view.get<TransformComponent, CameraComponent>(cameraEntity);

			if (camera.Primary)
			{
				mainCamera = &camera.Camera;
				cameraTransform = transform.GetTransform();
				break;
			}
			else 
			{
				TOAST_CORE_INFO("To add a planet a camera most be present");
				return;
			}
		}

		tc = entity.GetComponent<TransformComponent>();

		DirectX::XMVECTOR cameraPos, cameraRot, cameraScale;
		DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);

		PlanetSystem::GenerateDistanceLUT(component.MorphData.DistanceLUT, tc.Scale.x, mainCamera->GetPerspectiveVerticalFOV(), (float)(mViewportWidth), 200.0f, 8);
		PlanetSystem::GenerateFaceDotLevelLUT(component.FaceLevelDotLUT, tc.Scale.x, 8, component.PlanetData.maxAltitude.x);
		PlanetSystem::GeneratePatchGeometry(mc.Mesh->mPlanetVertices, mc.Mesh->mIndices, component.PatchLevels);
		PlanetSystem::GenerateBasePlanet(mc.Mesh->mPlanetFaces);
		PlanetSystem::GeneratePlanet(tc.GetTransform(), mc.Mesh->mPlanetFaces, mc.Mesh->mPlanetPatches, component.MorphData.DistanceLUT, component.FaceLevelDotLUT, cameraPos, component.Subdivisions);

		mc.Mesh->InitPlanet();
		//mc.Mesh->AddSubmesh(mc.Mesh->mIndices.size());
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
	void Scene::OnComponentAdded<DirectionalLightComponent>(Entity entity, DirectionalLightComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SkyLightComponent>(Entity entity, SkyLightComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
	{
	}

}