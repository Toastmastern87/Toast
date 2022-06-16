#include "tpch.h"
#include "Scene.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"
#include "Toast/Renderer/MeshFactory.h"

#include "Toast/Script/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"

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
	}

	Scene::~Scene()
	{
		sActiveScenes.erase(mSceneID);
	}

	Entity Scene::CreateEntity(const std::string& name, UUID parent)
	{
		Entity entity = { mRegistry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = {};

		auto& tc = entity.AddComponent<TransformComponent>();
		tc.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
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

		auto& tc = entity.AddComponent<TransformComponent>();
		tc.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
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
			mStats.TimeSteps += ts;
			if (mStats.TimeSteps > 0.1f)
			{
				mStats.FrameTime = ts.GetMilliseconds();
				mStats.TimeSteps -= 0.1f;
				mStats.FPS = 1.0f / ts.GetSeconds();
			}

			mStats.VerticesCount = 0;
		}

		// Update physics
		{
			//Gravity
			auto view = mRegistry.view<TransformComponent, RigidBodyComponent>();
			auto planetView = mRegistry.view<PlanetComponent>();
			for (auto entity : view)
			{
				//Only one planet can be handled at a time per scene
				auto pc = planetView.get<PlanetComponent>(planetView[0]);

				auto [tc, rbc] = view.get<TransformComponent, RigidBodyComponent>(entity);
				// Calculate linear velocity due to gravity
				DirectX::XMVECTOR pos = { 0.0f, 0.0f, 0.0f }, rot = { 0.0f, 0.0f, 0.0f }, scale = { 0.0f, 0.0f, 0.0f };
				DirectX::XMMatrixDecompose(&scale, &rot, &pos, tc.Transform);
				float mass = 1.0f / rbc.InvMass;
				DirectX::XMVECTOR impulseGravity = (-DirectX::XMVector3Normalize(pos) * (pc.PlanetData.gravAcc / 1000.0f) * mass * ts.GetSeconds());
				PhysicsEngine::ApplyImpulseLinear(rbc, impulseGravity);
				//TOAST_CORE_INFO("Linear Velocity: %f, %f, %f", rbc.LinearVelocity.x, rbc.LinearVelocity.y, rbc.LinearVelocity.z);
				
				// Update position due to gravity
				tc.Transform = DirectX::XMMatrixMultiply(tc.Transform, XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&rbc.LinearVelocity) * ts.GetSeconds()));
			}
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

				DirectX::XMFLOAT4 direction = { DirectX::XMVectorGetZ(transformComponent.Transform.r[0]), DirectX::XMVectorGetZ(transformComponent.Transform.r[1]), DirectX::XMVectorGetZ(transformComponent.Transform.r[2]), 0.0f, };
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
					cameraTransform = transform.Transform;

					break;
				}
				// if no camera is present nothing is rendered
				else
					return;
			}
		}

		if (mainCamera)
		{
			DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);
			DirectX::XMFLOAT4 cameraPosFloat;
			DirectX::XMStoreFloat4(&cameraPosFloat, cameraPos);

			DirectX::XMFLOAT4X4 fView, fInvView;
			DirectX::XMStoreFloat4x4(&fView, DirectX::XMMatrixInverse(nullptr, cameraTransform));
			DirectX::XMStoreFloat4x4(&fInvView, cameraTransform);
			mainCamera->SetViewMatrix(fView);
			mainCamera->SetInvViewMatrix(fInvView);

			// 3D Rendering
			Renderer::BeginScene(this, *mainCamera, cameraPosFloat);
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
						Renderer::SubmitSkybox(mSkybox, cameraPosFloat, mainCamera->GetViewMatrix(), mainCamera->GetProjection(), mEnvironmentIntensity, mSkyboxLod);
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
							Renderer::SubmitMesh(mesh.Mesh, transform.Transform, (int)entity, false);

							break;
						}
						case Settings::Wireframe::YES:
						{
							Renderer::SubmitMesh(mesh.Mesh, transform.Transform, (int)entity, true);

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
				auto viewPlanets = mRegistry.view<TransformComponent, PlanetComponent>();
				for (auto entity : viewPlanets)
				{
					auto [transform, planet] = viewPlanets.get<TransformComponent, PlanetComponent>(entity);

					switch (mSettings.WireframeRendering)
					{
					case Settings::Wireframe::NO:
					{
						if (planet.Mesh->mSubmeshes.size() > 0)
							Renderer::SubmitMesh(planet.Mesh, transform.Transform, (int)entity, false, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

						break;
					}
					case Settings::Wireframe::YES:
					{
						if (planet.Mesh->mSubmeshes.size() > 0)
							Renderer::SubmitMesh(planet.Mesh, transform.Transform, (int)entity, false, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

						break;
					}
					case Settings::Wireframe::ONTOP:
					{
						// TODO

						break;
					}
					}

					mStats.VerticesCount += static_cast<uint32_t>(planet.Mesh->GetPlanetVertices().size() * planet.Mesh->GetPlanetPatches().size());
				}

				Renderer::EndScene(false);
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
		else
			TOAST_CORE_ERROR("No main camera! Unable to render scene!");
	}

	void Scene::OnUpdateEditor(Timestep ts, const Ref<EditorCamera> editorCamera)
	{
		DirectX::XMVECTOR cameraPos = { 0.0f, 0.0f, 0.0f }, cameraRot = { 0.0f, 0.0f, 0.0f }, cameraScale = { 0.0f, 0.0f, 0.0f };
		DirectX::XMVECTOR cameraForward;
		DirectX::XMMATRIX cameraTransform = DirectX::XMMatrixIdentity();

		// Makes sure these values are not crap if there is no main camera in the scene, then it uses the editor camera instead
		cameraForward = DirectX::XMLoadFloat4(&editorCamera->GetForwardDirection());
		cameraPos = editorCamera->GetPosition();

		// Update statistics
		{
			mStats.TimeSteps += ts;
			if (mStats.TimeSteps > 0.1f)
			{
				mStats.FrameTime = ts.GetMilliseconds();
				mStats.TimeSteps -= 0.1f;
				mStats.FPS = 1.0f / ts.GetSeconds();
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

				DirectX::XMFLOAT4 direction = { DirectX::XMVectorGetZ(transformComponent.Transform.r[0]), DirectX::XMVectorGetZ(transformComponent.Transform.r[1]), DirectX::XMVectorGetZ(transformComponent.Transform.r[2]), 0.0f, };
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
				{
					cameraForward = { 0.0f, 0.0f, 1.0f };
					cameraTransform = transform.Transform;

					DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, transform.Transform);
					cameraForward = DirectX::XMVector3Rotate(cameraForward, cameraRot);
				}
			}

			if (!DirectX::XMVector4Equal(mOldCameraTransform.r[0], cameraTransform.r[0]) || !DirectX::XMVector4Equal(mOldCameraTransform.r[1], cameraTransform.r[1]) || !DirectX::XMVector4Equal(mOldCameraTransform.r[2], cameraTransform.r[2]) || !DirectX::XMVector4Equal(mOldCameraTransform.r[3], cameraTransform.r[3]))
			{
				InvalidateFrustum();

				auto view = mRegistry.view<PlanetComponent, TransformComponent>();
				for (auto entity : view)
				{
					auto [planet, transform] = view.get<PlanetComponent, TransformComponent>(entity);

					//TOAST_CORE_INFO("Camera Forward: %f, %f, %f", DirectX::XMVectorGetX(cameraForward), DirectX::XMVectorGetY(cameraForward), DirectX::XMVectorGetZ(cameraForward));
					PlanetSystem::GeneratePlanet(mFrustum.get(), transform.Transform, planet.Mesh->mPlanetFaces, planet.Mesh->mPlanetPatches, planet.DistanceLUT, planet.FaceLevelDotLUT, planet.HeightMultLUT, cameraPos, cameraForward, planet.Subdivisions, mSettings.BackfaceCulling, mSettings.FrustumCulling);

					planet.Mesh->InvalidatePlanet(true);
				}

				mOldCameraTransform = cameraTransform;
			}

			mOldCameraPos = cameraPos;
		}

		if (mOldBackfaceCullSetting != mSettings.BackfaceCulling || mOldFrustumCullSetting != mSettings.FrustumCulling)
		{
			auto view = mRegistry.view<PlanetComponent, TransformComponent>();
			for (auto entity : view)
			{
				auto [planet, transform] = view.get<PlanetComponent, TransformComponent>(entity);
				//TOAST_CORE_INFO("Camera Forward: %f, %f, %f", DirectX::XMVectorGetX(cameraForward), DirectX::XMVectorGetY(cameraForward), DirectX::XMVectorGetZ(cameraForward));
				PlanetSystem::GeneratePlanet(mFrustum.get(), transform.Transform, planet.Mesh->mPlanetFaces, planet.Mesh->mPlanetPatches, planet.DistanceLUT, planet.FaceLevelDotLUT, planet.HeightMultLUT, cameraPos, cameraForward, planet.Subdivisions, mSettings.BackfaceCulling, mSettings.FrustumCulling);
				
				planet.Mesh->InvalidatePlanet(true);
			}

			mOldBackfaceCullSetting = mSettings.BackfaceCulling;
			mOldFrustumCullSetting = mSettings.FrustumCulling;
		}

		DirectX::XMFLOAT4 cameraPosFloat;
		DirectX::XMStoreFloat4(&cameraPosFloat, editorCamera->GetPosition());

		// 3D Rendering
		Renderer::BeginScene(this, *editorCamera, cameraPosFloat);
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
						Renderer::SubmitMesh(mesh.Mesh, transform.Transform, (int)entity, false);

						break;
					}
					case Settings::Wireframe::YES:
					{
						Renderer::SubmitMesh(mesh.Mesh, transform.Transform, (int)entity, true);

						break;
					}
					case Settings::Wireframe::ONTOP:
					{
						// TODO

						break;
					}
					}
				}

				if (mSelectedEntity == entity)
					Renderer::SubmitSelecetedMesh(mesh.Mesh, transform.Transform);

				mStats.VerticesCount += static_cast<uint32_t>(mesh.Mesh->GetVertices().size());
			}

			// Planets!
			auto viewPlanets = mRegistry.view<TransformComponent, PlanetComponent>();
			for (auto entity : viewPlanets)
			{
				auto [transform, planet] = viewPlanets.get<TransformComponent, PlanetComponent>(entity);

				switch (mSettings.WireframeRendering)
				{
				case Settings::Wireframe::NO:
				{
					if(planet.Mesh->mSubmeshes.size() > 0)
						Renderer::SubmitMesh(planet.Mesh, transform.Transform, (int)entity, false, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

					break;
				}
				case Settings::Wireframe::YES:
				{
					if (planet.Mesh->mSubmeshes.size() > 0)
						Renderer::SubmitMesh(planet.Mesh, transform.Transform, (int)entity, true, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

					break;
				}
				case Settings::Wireframe::ONTOP:
				{
					// TODO

					break;
				}
				}

				if (mSelectedEntity == entity)
					Renderer::SubmitSelecetedMesh(planet.Mesh, transform.Transform);

				mStats.VerticesCount += static_cast<uint32_t>(planet.Mesh->GetPlanetVertices().size() * planet.Mesh->GetPlanetPatches().size());
			}

			Renderer::EndScene(true);
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
			auto view = mRegistry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				DirectX::XMVECTOR scale, rotation, translation;
				DirectX::XMFLOAT3 translationFloat3;
				DirectX::XMMatrixDecompose(&scale, &rotation, &translation, transform.Transform);
				DirectX::XMStoreFloat3(&translationFloat3, translation);

				if (mSettings.CameraFrustum)
					RendererDebug::SubmitCameraFrustum(camera.Camera, transform.Transform, translationFloat3);
			}

			if (mSettings.Grid)
				RendererDebug::SubmitGrid(*editorCamera);
		}
		RendererDebug::EndScene(true);
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

	void Scene::SetSkybox(Ref<TextureCube> skybox)
	{
		mSkyboxTexture = skybox;
		mSkyboxMaterial->SetTexture(7, D3D11_PIXEL_SHADER, mSkyboxTexture.get());
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
		target->mSkybox = mSkybox;

		std::unordered_map<UUID, entt::entity> enttMap;
		auto idComponent = mRegistry.view<IDComponent>();
		for (auto entity : idComponent)
		{
			auto uuid = mRegistry.get<IDComponent>(entity).ID;
			Entity e = target->CreateEntityWithID(uuid, "");
			enttMap[uuid] = e.mEntityHandle;
		}

		// Frustum
		target->mFrustum = mFrustum;

		CopyComponent<TagComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<TransformComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<MeshComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<PlanetComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<CameraComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<DirectionalLightComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SkyLightComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<ScriptComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<RigidBodyComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SphereColliderComponent>(target->mRegistry, mRegistry, enttMap);

		const auto& entityInstanceMap = ScriptEngine::GetEntityInstanceMap();
		if (entityInstanceMap.find(target->GetUUID()) != entityInstanceMap.end())
			ScriptEngine::CopyEntityScriptData(target->GetUUID(), mSceneID);
		else
			TOAST_CORE_WARN("NO Data being copied");
	}

	void Scene::InvalidateFrustum()
	{
		DirectX::XMVECTOR cameraScale, cameraRot, cameraPos;

		auto view = mRegistry.view<TransformComponent, CameraComponent>();
		for (auto entity : view)
		{
			auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

			DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, transform.Transform);

			if (camera.Primary) 
				mFrustum->Update(transform.Transform, camera.Camera.GetAspecRatio(), camera.Camera.GetPerspectiveVerticalFOV(), camera.Camera.GetNearClip(), camera.Camera.GetFarClip(), cameraPos);
		}
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

		component.Mesh = CreateRef<Mesh>();
		component.Mesh->SetMaterial(MaterialLibrary::Get("Planet"));
		component.Mesh->mTopology = PrimitiveTopology::TRIANGLELIST;
		component.Mesh->SetIsPlanet(true);

		SceneCamera* mainCamera = nullptr;
		DirectX::XMMATRIX cameraTransform;
		auto view = mRegistry.view<TransformComponent, CameraComponent>();
		for (auto cameraEntity : view)
		{
			auto [transform, camera] = view.get<TransformComponent, CameraComponent>(cameraEntity);

			if (camera.Primary)
			{
				mainCamera = &camera.Camera;
				cameraTransform = transform.Transform;
				break;
			}
			else
			{
				TOAST_CORE_INFO("To add a planet a camera most be present");
				return;
			}
		}

		tc = entity.GetComponent<TransformComponent>();

		DirectX::XMVECTOR cameraPos, cameraRot, cameraScale, cameraForward;
		if (mainCamera)
		{
			cameraForward = { 0.0f, 0.0f, 1.0f };
			DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);
			cameraForward = DirectX::XMVector3Rotate(cameraForward, cameraRot);
		}
		else
		{
			cameraForward = { 0.0f, 0.0f, 1.0f };
			cameraPos = { 0.0f, 0.0f, 0.0f };
		}

		DirectX::XMVECTOR scale, rotation, translation;
		DirectX::XMMatrixDecompose(&scale, &rotation, &translation, tc.Transform);

		InvalidateFrustum();

		PlanetSystem::GenerateBasePlanet(component.Mesh->mPlanetFaces);
		PlanetSystem::GeneratePatchGeometry(component.Mesh->mPlanetVertices, component.Mesh->mIndices, component.PatchLevels);

		PlanetSystem::GenerateDistanceLUT(component.DistanceLUT, 8);
		PlanetSystem::GenerateFaceDotLevelLUT(component.FaceLevelDotLUT, DirectX::XMVectorGetX(scale), 8, component.PlanetData.maxAltitude);
		PlanetSystem::GenerateHeightMultLUT(component.Mesh->mPlanetFaces, component.HeightMultLUT, DirectX::XMVectorGetX(scale), 8, component.PlanetData.maxAltitude, tc.Transform);

		PlanetSystem::GeneratePlanet(mFrustum.get(), tc.Transform, component.Mesh->mPlanetFaces, component.Mesh->mPlanetPatches, component.DistanceLUT, component.FaceLevelDotLUT, component.HeightMultLUT, cameraPos, cameraPos, component.Subdivisions, mSettings.BackfaceCulling, mSettings.FrustumCulling);

		component.Mesh->InvalidatePlanet(true);
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		auto tc = entity.GetComponent<TransformComponent>();

		component.Camera.SetViewportSize(mViewportWidth, mViewportHeight);

		mFrustum = CreateRef<Frustum>();
		InvalidateFrustum();
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
		// Initiate the skybox
		Shader* skyboxShader = ShaderLibrary::Load("assets/shaders/Skybox.hlsl");
		mSkyboxMaterial = CreateRef<Material>("Skybox", skyboxShader);
		mSkybox = CreateRef<Mesh>("..\\Toaster\\assets\\meshes\\Cube.gltf", true);
		mSkybox->SetMaterial(mSkyboxMaterial);
	}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<RigidBodyComponent>(Entity entity, RigidBodyComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SphereColliderComponent>(Entity entity, SphereColliderComponent& component)
	{
	}

}