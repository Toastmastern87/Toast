#include "tpch.h"
#include "Scene.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"
#include "Toast/Renderer/MeshFactory.h"
#include "Toast/Renderer/PlanetSystem.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"

namespace Toast {

	std::unordered_map<UUID, Scene*> sActiveScenes;

	struct SceneComponent
	{
		UUID SceneID;
	};

	Scene::Scene()
	{
		mSceneEntity = mRegistry.create();
		mRegistry.emplace<SceneComponent>(mSceneEntity, mSceneID);

		sActiveScenes[mSceneID] = this;

		mParticleSystem = CreateRef<ParticleSystem>();

		mParticleSystem->Initialize();
	}

	Scene::~Scene()
	{
		auto view = mRegistry.view<PlanetComponent, TransformComponent>();
		for (auto entity : view)
		{
			auto [planet, planetTransform] = view.get<PlanetComponent, TransformComponent>(entity);

			PlanetSystem::Shutdown();
		}

		sActiveScenes.erase(mSceneID);
	}

	Entity Scene::CreateEntity(const std::string& name, UUID parent)
	{
		Entity entity = { mRegistry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = {};

		auto& tc = entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		entity.AddComponent<RelationshipComponent>();

		mEntityIDMap[idComponent.ID] = entity;

		return entity;
	}

	Entity Scene::CreateEntityWithID(UUID uuid, const std::string& name)
	{
		Entity entity = { mRegistry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = uuid;

		auto& tc = entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		entity.AddComponent<RelationshipComponent>();

		TOAST_CORE_ASSERT(mEntityIDMap.find(uuid) == mEntityIDMap.end(), "Entity already exist!");
		mEntityIDMap[uuid] = entity;

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		mEntityIDMap.erase(entity.GetUUID());
		mRegistry.destroy(entity);
	}

	void Scene::OnRuntimeStart()
	{
		// Scripting
		{
			ScriptEngine::OnRuntimeStart(this);
			// Instantiate all script entities

			auto view = mRegistry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::OnCreateEntity(e);
			}
		}

		mIsRunning = true;
	}

	void Scene::OnRuntimeStop()
	{
		mIsRunning = false;

		ScriptEngine::OnRuntimeStop();

		auto view = mRegistry.view<TransformComponent, MeshComponent>();
		for (auto entity : view)
		{
			auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);

			if (mesh.MeshObject->GetIsAnimated())
				mesh.MeshObject->ResetAnimations();
		}
	}

	void Scene::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseButtonPressedEvent>(TOAST_BIND_EVENT_FN(Scene::OnMouseButtonPressed));
		dispatcher.Dispatch<MouseButtonReleasedEvent>(TOAST_BIND_EVENT_FN(Scene::OnMouseButtonReleased));
	}

	bool Scene::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{	
		// Check that a valid entity is being hovered over by the mouse
		if (mHoveredEntity != entt::null) 
		{
			Entity entity = { mHoveredEntity, this };

			if (entity.HasComponent<ScriptComponent>())
				ScriptEngine::OnEventEntity(entity);	
		}

		return true;
	}

	bool Scene::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
	{
		return true;
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		TOAST_PROFILE_FUNCTION();

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

		if (!mIsPaused) 
		{
			// Updating box colliders
			{
				auto view = mRegistry.view<BoxColliderComponent, TransformComponent>();
				for (auto entity : view)
				{
					auto [bcc, tc] = view.get<BoxColliderComponent, TransformComponent>(entity);
					DirectX::XMVECTOR totalRotVec = DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&tc.RotationQuaternion), DirectX::XMQuaternionRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&tc.RotationEulerAngles)));
					DirectX::XMFLOAT4 totalRot;
					DirectX::XMStoreFloat4(&totalRot, totalRotVec);;
				}
			}

			double deltaTime = ts.GetSeconds() * mTimeScale;
			double targetFrameTime = 1.0 / mSettings.PhysicsFPS; // Fixed time step in seconds

			mSettings.physicsElapsedTime += deltaTime;

			int maxPhysicsUpdatesPerFrame = 1; // Prevents spiral of death
			int physicsUpdateCount = 0;

			while (mSettings.physicsElapsedTime >= targetFrameTime && physicsUpdateCount < maxPhysicsUpdatesPerFrame)
			{
				//TOAST_CORE_CRITICAL("Elapsed time: %lf", mSettings.physicsElapsedTime);

				// Call PhysicsEngine::Update with the fixed time step
				PhysicsEngine::Update(&mRegistry, this, targetFrameTime, mSettings.PhysicSlowmotion, 1);

				mSettings.physicsElapsedTime -= targetFrameTime;
				physicsUpdateCount++;
			}

			if (physicsUpdateCount == maxPhysicsUpdatesPerFrame)
			{
				mSettings.physicsElapsedTime = 0.0;
			}

			// Scripting
			{
				// C# Entity OnUpdate
				auto view = mRegistry.view<ScriptComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };
					ScriptEngine::OnUpdateEntity(e, ts * mTimeScale);
				}
			}
		}

		// Process Lights
		{
			mLightEnvironment = LightEnvironment();
			auto lights = mRegistry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
			uint32_t directionalLightIndex = 0;
			for (auto entity : lights)
			{
				auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);

				mLightEnvironment = LightEnvironment();
				auto lights = mRegistry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
				uint32_t directionalLightIndex = 0;
				for (auto entity : lights)
				{
					auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);

					DirectX::XMMATRIX transform = transformComponent.GetTransform();

					// Extract the forward vector (Z-axis)
					DirectX::XMVECTOR lightDir = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(transform.r[2]));

					DirectX::XMFLOAT4 direction;
					DirectX::XMStoreFloat4(&direction, lightDir);
					direction.w = 0.0f;
					DirectX::XMFLOAT4 radiance = DirectX::XMFLOAT4(lightComponent.Radiance.x, lightComponent.Radiance.y, lightComponent.Radiance.z, 0.0f);

					float orthoWidth = mSettings.SunFrustumOrthoSize;
					float orthoHeight = mSettings.SunFrustumOrthoSize;
					float orthoNear = 0.1f;
					float orthoFar = lightComponent.SunDesiredCoverage;

					// Create the orthographic projection matrix for the light
					DirectX::XMMATRIX lightProj = XMMatrixOrthographicLH(orthoWidth, orthoHeight, orthoNear, orthoFar);

					// Position the light to cover the area around the origin
					DirectX::XMVECTOR lightPos = DirectX::XMVectorSubtract(DirectX::XMVectorZero(), DirectX::XMVectorScale(lightDir, lightComponent.SunLightDistance));

					DirectX::XMVECTOR defaultUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
					DirectX::XMVECTOR right = DirectX::XMVector3Cross(defaultUp, lightDir);
					// Check if the right vector is valid (not zero length)
					float rightLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(right));
					if (rightLengthSq < 1e-6f)
					{
						// If invalid, choose a different default up vector (e.g., Z-axis)
						defaultUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
						right = DirectX::XMVector3Cross(defaultUp, lightDir);
					}
					right = DirectX::XMVector3Normalize(right);

					// Recompute the up vector to be orthogonal to the light direction and right vector
					DirectX::XMVECTOR up = DirectX::XMVector3Cross(lightDir, right);
					up = DirectX::XMVector3Normalize(up);

					DirectX::XMMATRIX lightView = DirectX::XMMatrixLookToLH(lightPos, lightDir, up);
					DirectX::XMMATRIX invLightView = DirectX::XMMatrixInverse(nullptr, lightView);
					DirectX::XMMATRIX lightViewProj = XMMatrixMultiply(lightView, lightProj);

					mLightEnvironment.DirectionalLights[directionalLightIndex++] =
					{
						lightViewProj,
						direction,
						radiance,
						lightComponent.Intensity
					};
				}
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
			}
		}

		// Process Animations
		auto view = mRegistry.view<TransformComponent, MeshComponent>();
		for (auto entity : view)
		{
			auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
			
			if (mesh.MeshObject->GetIsAnimated())
				mesh.MeshObject->OnUpdate(ts * mTimeScale);
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

		if (mainCamera)
		{
			// Updated Meshes to check which LOD Group it should use during the rendering.
			{
				auto view = mRegistry.view<MeshComponent, TransformComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };

					MeshComponent& mc = e.GetComponent<MeshComponent>();
					TransformComponent& tc = e.GetComponent<TransformComponent>();

					if (mc.MeshObject->HasLODGroups())
					{
						double maxDistance = 10000.0;
						double distance = Vector3::Length(tc.Translation);
						double remappedDistance = std::clamp(distance / maxDistance, 0.0, 1.0);
						mc.MeshObject->UpdateLODDistance(remappedDistance);

						std::vector<float> thresholds = mc.MeshObject->GetLODThresholds();

						int activeLOD = 0; // Default to LOD0

						if (remappedDistance > thresholds[1])
							activeLOD = 2; // LOD2
						else if (remappedDistance > thresholds[0])
							activeLOD = 1; // LOD1

						mc.MeshObject->SetActiveLODGroup(activeLOD);
					}
				}
			}

			// Process Particles
			{
				size_t maxParticleCount = 0;

				int32_t nrOfParticles = 0;
				auto view = mRegistry.view<ParticlesComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };

					ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();

					maxParticleCount += (pc.MaxLifeTime / pc.SpawnDelay) + 1;
					nrOfParticles += pc.Particles.size();
				}

				Renderer::InvalidateParticleBuffers(nrOfParticles, maxParticleCount);

				for (auto entity : view)
				{
					Entity e = { entity, this };

					ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();
					TransformComponent& tc = e.GetComponent<TransformComponent>();

					DirectX::XMFLOAT3 spawnPosition = e.GetComponent<TransformComponent>().Translation;

					DirectX::XMFLOAT3 finalVelocity = { 0.0f, 0.0f, 0.0f };

					DirectX::XMMATRIX rotationMatrix = tc.GetRotation();
					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());

						auto parentTC = parent.GetComponent<TransformComponent>();

						RigidBodyComponent parentRB;

						if (parent.HasComponent<RigidBodyComponent>())
							parentRB = parent.GetComponent<RigidBodyComponent>();

						DirectX::XMMATRIX parentTransform = parentTC.GetTransformWithoutScale();

						finalVelocity = { pc.Velocity.x + (float)parentRB.LinearVelocity.x, pc.Velocity.y + (float)parentRB.LinearVelocity.y, pc.Velocity.z + (float)parentRB.LinearVelocity.z };

						// Transform the local spawn position by the parent's transform.
						DirectX::XMVECTOR localPos = DirectX::XMLoadFloat3(&spawnPosition);
						DirectX::XMVECTOR worldPos = DirectX::XMVector3Transform(localPos, parentTransform);
						DirectX::XMStoreFloat3(&spawnPosition, worldPos);

						rotationMatrix = DirectX::XMMatrixMultiply(rotationMatrix, parentTC.GetRotation());
					}

					Renderer::SetParticleMaskTexture(pc.MaskTexture);

					mParticleSystem->OnUpdate(ts, pc, spawnPosition, tc.Scale, rotationMatrix, maxParticleCount, finalVelocity);
				}

				// Gather particles from all Particle Systems
				std::vector<Particle> aggregatedParticles;
				for (auto entity : view)
				{
					Entity e = { entity, this };
					ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();
					aggregatedParticles.insert(aggregatedParticles.end(), pc.Particles.begin(), pc.Particles.end());
				}

				Renderer::FillParticleBuffer(aggregatedParticles);
			}

			// Rebuild planet if needed
			auto view = mRegistry.view<PlanetComponent, TransformComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };

				TerrainDetailComponent* tdc = nullptr;
				TerrainColliderComponent* tcc = nullptr;
				PlanetComponent& pc = e.GetComponent<PlanetComponent>();
				TransformComponent& tc = e.GetComponent<TransformComponent>();

				if(e.HasComponent<TerrainDetailComponent>())
					tdc = &e.GetComponent<TerrainDetailComponent>();

				if (e.HasComponent<TerrainColliderComponent>())
					tcc = &e.GetComponent<TerrainColliderComponent>();

				DirectX::XMVECTOR cameraForward = { 0.0f, 0.0f, 1.0f };
				DirectX::XMVECTOR cameraPos, cameraRot, cameraScale;

				DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);
				cameraForward = DirectX::XMVector3Rotate(cameraForward, cameraRot);

				InvalidateFrustum();

				DirectX::XMMATRIX noScaleModelMatrix = DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(tc.RotationEulerAngles.x), DirectX::XMConvertToRadians(tc.RotationEulerAngles.y), DirectX::XMConvertToRadians(tc.RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.RotationQuaternion))
					* DirectX::XMMatrixTranslation(tc.Translation.x, tc.Translation.y, tc.Translation.z);

				// Starting new thread to create a new planet if one isn't already being created
				PlanetSystem::RegeneratePlanet(mFrustum, tc.Scale, tc.Translation, noScaleModelMatrix, cameraPos, mSettings.BackfaceCulling, mSettings.FrustumCulling, pc, tcc->BuildColliders, tcc->BuildColliderPositions, tdc);

				PlanetSystem::UpdatePlanet(pc.RenderMesh, pc.BuildVertices, pc.BuildIndices, *tcc);
			}

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
					if (mEnvironment.RadianceMap)
						Renderer::SubmitSkybox(cameraPosFloat, mainCamera->GetViewMatrix(), mainCamera->GetProjection(), mEnvironmentIntensity, mSkyboxLod);
				}

				// Meshes!
				auto viewMeshes = mRegistry.view<TransformComponent, MeshComponent>();
				for (auto entity : viewMeshes)
				{
					auto [transform, mesh] = viewMeshes.get<TransformComponent, MeshComponent>(entity);

					//Do not submit mesh if it's a planet
					//if (!mesh.MeshObject->GetIsPlanet())
					//{
						switch (mSettings.WireframeRendering)
						{
						case Settings::Wireframe::NO:
						{
							Renderer::SubmitMesh(mesh.MeshObject, transform.GetTransform(), (int)entity, false, 0);

							break;
						}
						case Settings::Wireframe::YES:
						{
							Renderer::SubmitMesh(mesh.MeshObject, transform.GetTransform(), (int)entity, true, 0);

							break;
						}
						case Settings::Wireframe::ONTOP:
						{
							// TODO

							break;
						}
						}
				//	}

					mStats.VerticesCount += static_cast<uint32_t>(mesh.MeshObject->GetVertices().size());
				}

				auto terrainObjectMeshes = mRegistry.view<TransformComponent, TerrainObjectComponent>();
				for (auto entity : terrainObjectMeshes)
				{
					auto [transform, terrainObject] = terrainObjectMeshes.get<TransformComponent, TerrainObjectComponent>(entity);
					switch (mSettings.WireframeRendering)
					{
					case Settings::Wireframe::NO:
					{
						Renderer::SubmitMesh(terrainObject.MeshObject, transform.GetTransform(), (int)entity, false, 0);

						break;
					}
					case Settings::Wireframe::YES:
					{
						Renderer::SubmitMesh(terrainObject.MeshObject, transform.GetTransform(), (int)entity, true, 0);

						break;
					}
					}

					// TODO fix the count number for vertices
					//mStats.VerticesCount += static_cast<uint32_t>(terrainObject.MeshObject->GetVertices().size() * terrainObject.);
				}

				// Planets!
				auto viewPlanets = mRegistry.view<TransformComponent, PlanetComponent>();
				for (auto entity : viewPlanets)
				{
					auto [transform, planet] = viewPlanets.get<TransformComponent, PlanetComponent>(entity);

					planet.PlanetData.planetCenter = transform.Translation;

					switch (mSettings.WireframeRendering)
					{
					case Settings::Wireframe::NO:
					{
						if (planet.RenderMesh->mLODGroups[0]->Submeshes.size() > 0)
							Renderer::SubmitMesh(planet.RenderMesh, DirectX::XMMatrixIdentity(), (int)entity, false, 1, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

						break;
					}
					case Settings::Wireframe::YES:
					{
						if (planet.RenderMesh->mLODGroups[0]->Submeshes.size() > 0)
							Renderer::SubmitMesh(planet.RenderMesh, DirectX::XMMatrixIdentity(), (int)entity, false, 1, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

						break;
					}
					case Settings::Wireframe::ONTOP:
					{
						// TODO

						break;
					}
					}

					mStats.VerticesCount += static_cast<uint32_t>(planet.RenderMesh->GetVertices().size());
				}

				Renderer::EndScene(true, mSettings.Shadows, mSettings.SSAO, mSettings.DynamicIBL, *mainCamera, cameraPosFloat);
			}

			// Debug Rendering
			RendererDebug::BeginScene(*mainCamera);
			{
				// Colliders
				auto entities = mRegistry.view<TransformComponent>();
				for (auto entity : entities)
				{
					DirectX::XMVECTOR pos = { 0.0f, 0.0f, 0.0f }, rot = { 0.0f, 0.0f, 0.0f }, scale = { 0.0f, 0.0f, 0.0f };

					Entity e{ entity, this };

					auto tc = e.GetComponent<TransformComponent>();
					DirectX::XMMatrixDecompose(&scale, &rot, &pos, tc.GetTransform());

					bool hasSphereCollider = e.HasComponent<SphereColliderComponent>();
					bool hasBoxCollider = e.HasComponent<BoxColliderComponent>();

					Ref<Mesh> colliderMesh;
					bool renderCollider = false;

					if (hasSphereCollider)
					{
						auto scc = e.GetComponent<SphereColliderComponent>();
						scale = { (float)scc.Collider->mRadius, (float)scc.Collider->mRadius, (float)scc.Collider->mRadius };
						colliderMesh = scc.ColliderMesh;
						renderCollider = scc.RenderCollider;
					}
					else if (hasBoxCollider)
					{
						auto bcc = e.GetComponent<BoxColliderComponent>();
						scale = { (float)bcc.Collider->mSize.x, (float)bcc.Collider->mSize.y, (float)bcc.Collider->mSize.z };
						colliderMesh = bcc.ColliderMesh;
						renderCollider = bcc.RenderCollider;
					}

					DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScalingFromVector(scale) * DirectX::XMMatrixRotationQuaternion(rot) * DirectX::XMMatrixTranslationFromVector(pos);

					if (renderCollider && mSettings.RenderColliders)
						RendererDebug::SubmitMesh(colliderMesh, transform, false);
				}
			}
			RendererDebug::EndScene(true, true, true, false);

			// 2D UI Rendering
			Renderer2D::BeginScene(*mainCamera);
			{
				DirectX::XMFLOAT3 finalPosition;

				//Panels
				auto uiPanelEntites = mRegistry.view<TransformComponent, UIPanelComponent>();

				for (auto entity : uiPanelEntites)
				{
					auto [tc, upc] = uiPanelEntites.get<TransformComponent, UIPanelComponent>(entity);

					Entity e{ entity, this };

					const auto& name = e.GetComponent<TagComponent>().Tag;

					if (upc.Panel->GetVisible())
					{
						if (e.HasParent())
						{
							Entity parent = FindEntityByUUID(e.GetParentUUID());
							bool is2DParent = parent.HasComponent<UIPanelComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UITextComponent>();

							auto& parentTC = parent.GetComponent<TransformComponent>();

							DirectX::XMVECTOR parentWorldPos = XMLoadFloat3(&parentTC.Translation);
							DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&mainCamera->GetViewMatrix());
							DirectX::XMMATRIX projectionMatrix = DirectX::XMLoadFloat4x4(&mainCamera->GetProjection());

							DirectX::XMFLOAT3 parentScreenPos;

							DirectX::XMVECTOR parentViewPos = DirectX::XMVector3Transform(parentWorldPos, viewMatrix);
							DirectX::XMVECTOR clipSpacePos = DirectX::XMVector3Transform(parentViewPos, projectionMatrix);

							float x = clipSpacePos.m128_f32[0];
							float y = clipSpacePos.m128_f32[1];
							float z = clipSpacePos.m128_f32[2];
							float w = clipSpacePos.m128_f32[3];

							float ndcX = x / w;
							float ndcY = y / w;
							float ndcZ = z / w;

							float screenX = (ndcX * 0.5f + 0.5f) * mViewportWidth;
							float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * mViewportHeight;

							parentScreenPos.x = screenX - (mViewportWidth / 2.0f);
							parentScreenPos.y = -(screenY - (mViewportHeight / 2.0f));
							parentScreenPos.z = ndcZ;

							if (is2DParent)
							{
								DirectX::XMFLOAT3 parentPosition = FindEntityByUUID(e.GetParentUUID()).GetComponent<TransformComponent>().Translation;
								DirectX::XMFLOAT3 position = tc.Translation;
								finalPosition = { position.x + parentPosition.x, position.y + parentPosition.y, 1.0f };
							}
							else
								finalPosition = { tc.Translation.x , tc.Translation.y, 1.0f };

							if (upc.Panel->GetConnectToParent())
								Renderer2D::SubmitConnector(tc.Translation, tc.Scale, *upc.Panel->GetCornerRadius(), parentScreenPos, 3.0f, *upc.Panel->GetBorderSize());
						}
						else
							finalPosition = { tc.Translation.x , tc.Translation.y, 1.0f };

						std::string panelTextureName;
						if (!upc.Panel->GetUseColor())
							panelTextureName = upc.Panel->GetTextureFilepath();

						Renderer2D::SubmitPanel(finalPosition, { tc.Scale.x, tc.Scale.y, *upc.Panel->GetCornerRadius(), *upc.Panel->GetBorderSize() }, upc.Panel->GetColorF4(), (int)entity, !upc.Panel->GetUseColor(), panelTextureName, false);
					}
				}

				//Buttons
				auto uiButtonEntites = mRegistry.view<TransformComponent, UIButtonComponent>();

				for (auto entity : uiButtonEntites)
				{
					auto [tc, ubc] = uiButtonEntites.get<TransformComponent, UIButtonComponent>(entity);

					Entity e{ entity, this };

					bool renderButton = true;

					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());

						if (parent.HasComponent<UIPanelComponent>())
						{
							UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();

							renderButton = parentPanel.Panel->GetVisible();
						}

						DirectX::XMFLOAT3 parentPosition = FindEntityByUUID(e.GetParentUUID()).GetComponent<TransformComponent>().Translation;
						DirectX::XMFLOAT3 position = tc.Translation;
						finalPosition = { position.x + parentPosition.x, position.y + parentPosition.y, 1.0f };
					}
					else
						finalPosition = { tc.Translation.x , tc.Translation.y, 1.0f };

					if (renderButton)
						Renderer2D::SubmitButton(finalPosition, { tc.Scale.x, tc.Scale.y, *ubc.Button->GetCornerRadius(), 1.0f }, ubc.Button->GetColorF4(), (int)entity, !ubc.Button->GetUseColor(), false);
				}

				//Texts
				auto uiTextEntites = mRegistry.view<TransformComponent, UITextComponent>();

				for (auto entity : uiTextEntites)
				{
					auto [tc, uitc] = uiTextEntites.get<TransformComponent, UITextComponent>(entity);

					Entity e{ entity, this };

					bool renderText = true;

					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());

						if (parent.HasComponent<UIPanelComponent>())
						{
							UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();

							renderText = parentPanel.Panel->GetVisible();
						}

						DirectX::XMFLOAT3 parentPosition = FindEntityByUUID(e.GetParentUUID()).GetComponent<TransformComponent>().Translation;
						DirectX::XMFLOAT3 position = tc.Translation;
						finalPosition = { position.x + parentPosition.x, position.y + parentPosition.y, 2.0f };
					}
					else
						finalPosition = { tc.Translation.x , tc.Translation.y, 2.0f };

					if (renderText)
						Renderer2D::SubmitText(finalPosition, { tc.Scale.x, tc.Scale.y, 1.0f, 1.0f }, uitc.Text, (int)entity, true);
				}
			}
			Renderer2D::EndScene();
		}
		else 
			TOAST_CORE_ERROR("No main camera! Unable to render scene!");
	}

	void Scene::OnUpdateEditor(Timestep ts, const Ref<EditorCamera> editorCamera)
	{
		entt::entity* mainCamera = nullptr;
		TransformComponent* mainCameraTransform;
		CameraComponent* mainCameraComponent;
		{
			auto view = mRegistry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto camera = view.get<CameraComponent>(entity);

				if (camera.Primary) 
				{
					mainCamera = &entity;
					mainCameraTransform = &view.get<TransformComponent>(entity);
					mainCameraComponent = &view.get<CameraComponent>(entity);
				}

				if (mainCameraTransform->IsDirty)
				{
					InvalidateFrustum();

					mInvalidatePlanet = true;
					mainCameraTransform->IsDirty = false;
				}
			}
		}

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
		// Frustum corners in light's view space
		DirectX::XMVECTOR frustumCorners[8];

		// Process lights
		{
			mLightEnvironment = LightEnvironment();
			auto lights = mRegistry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
			uint32_t directionalLightIndex = 0;
			for (auto entity : lights)
			{
				auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);

				DirectX::XMMATRIX transform = transformComponent.GetTransform();

				// Extract the forward vector (Z-axis)
				DirectX::XMVECTOR lightDir = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(transform.r[2]));

				DirectX::XMFLOAT4 direction;
				DirectX::XMStoreFloat4(&direction, lightDir);
				direction.w = 0.0f;
				DirectX::XMFLOAT4 radiance = DirectX::XMFLOAT4(lightComponent.Radiance.x, lightComponent.Radiance.y, lightComponent.Radiance.z, 0.0f);
				
				float orthoWidth = mSettings.SunFrustumOrthoSize;    
				float orthoHeight = mSettings.SunFrustumOrthoSize;    
				float orthoNear = 0.1f;
				float orthoFar = lightComponent.SunDesiredCoverage;

				if (mSettings.SunLightFrustum)
				{
					// Calculate half dimensions
					float halfWidth = orthoWidth / 2.0f;
					float halfHeight = orthoHeight / 2.0f;

					// Near plane
					frustumCorners[0] = DirectX::XMVectorSet(-halfWidth, -halfHeight, orthoNear, 1.0f); // Near Bottom Left
					frustumCorners[1] = DirectX::XMVectorSet(halfWidth, -halfHeight, orthoNear, 1.0f);  // Near Bottom Right
					frustumCorners[2] = DirectX::XMVectorSet(halfWidth, halfHeight, orthoNear, 1.0f);   // Near Top Right
					frustumCorners[3] = DirectX::XMVectorSet(-halfWidth, halfHeight, orthoNear, 1.0f);  // Near Top Left

					// Far plane
					frustumCorners[4] = DirectX::XMVectorSet(-halfWidth, -halfHeight, orthoFar, 1.0f);  // Far Bottom Left
					frustumCorners[5] = DirectX::XMVectorSet(halfWidth, -halfHeight, orthoFar, 1.0f);   // Far Bottom Right
					frustumCorners[6] = DirectX::XMVectorSet(halfWidth, halfHeight, orthoFar, 1.0f);    // Far Top Right
					frustumCorners[7] = DirectX::XMVectorSet(-halfWidth, halfHeight, orthoFar, 1.0f); // Far Top Left
				}

				// Create the orthographic projection matrix for the light
				DirectX::XMMATRIX lightProj = XMMatrixOrthographicLH(orthoWidth, orthoHeight, orthoNear, orthoFar);

				// Position the light to cover the area around the origin
				DirectX::XMVECTOR lightPos = DirectX::XMVectorSubtract(DirectX::XMVectorZero(), DirectX::XMVectorScale(lightDir, lightComponent.SunLightDistance));

				DirectX::XMVECTOR defaultUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				DirectX::XMVECTOR right = DirectX::XMVector3Cross(defaultUp, lightDir);
				// Check if the right vector is valid (not zero length)
				float rightLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(right));
				if (rightLengthSq < 1e-6f)
				{
					// If invalid, choose a different default up vector (e.g., Z-axis)
					defaultUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
					right = DirectX::XMVector3Cross(defaultUp, lightDir);
				}
				right = DirectX::XMVector3Normalize(right);

				// Recompute the up vector to be orthogonal to the light direction and right vector
				DirectX::XMVECTOR up = DirectX::XMVector3Cross(lightDir, right);
				up = DirectX::XMVector3Normalize(up);

				DirectX::XMMATRIX lightView = DirectX::XMMatrixLookToLH(lightPos, lightDir, up);
				DirectX::XMMATRIX invLightView = DirectX::XMMatrixInverse(nullptr, lightView);
				DirectX::XMMATRIX lightViewProj = XMMatrixMultiply(lightView, lightProj);

				if (mSettings.SunLightFrustum)
				{
					// Transform corners to world space
					for (int i = 0; i < 8; ++i)
						frustumCorners[i] = DirectX::XMVector4Transform(frustumCorners[i], invLightView);
				}

				mLightEnvironment.DirectionalLights[directionalLightIndex++] =
				{
					lightViewProj,
					direction,
					radiance,
					lightComponent.Intensity
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
			}
		}

		// Process Particles
		{
			size_t maxParticleCount = 0;

			int32_t nrOfParticles = 0;
			auto view = mRegistry.view<ParticlesComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };

				ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();

				maxParticleCount += (pc.MaxLifeTime / pc.SpawnDelay) + 1;
				nrOfParticles += pc.Particles.size();
			}

			Renderer::InvalidateParticleBuffers(nrOfParticles, maxParticleCount);

			for (auto entity : view)
			{
				Entity e = { entity, this };

				ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();
				TransformComponent& tc = e.GetComponent<TransformComponent>();

				DirectX::XMFLOAT3 spawnPosition = e.GetComponent<TransformComponent>().Translation;

				DirectX::XMMATRIX rotationMatrix = tc.GetRotation();
				if (e.HasParent())
				{
					Entity parent = FindEntityByUUID(e.GetParentUUID());

					auto parentTC = parent.GetComponent<TransformComponent>();

					DirectX::XMMATRIX parentTransform = parentTC.GetTransformWithoutScale();

					// Transform the local spawn position by the parent's transform.
					DirectX::XMVECTOR localPos = DirectX::XMLoadFloat3(&spawnPosition);
					DirectX::XMVECTOR worldPos = DirectX::XMVector3Transform(localPos, parentTransform);
					DirectX::XMStoreFloat3(&spawnPosition, worldPos);

					rotationMatrix = DirectX::XMMatrixMultiply(rotationMatrix, parentTC.GetRotation());
				}

				Renderer::SetParticleMaskTexture(pc.MaskTexture);

				mParticleSystem->OnUpdate(ts, pc, spawnPosition, tc.Scale, rotationMatrix, maxParticleCount, pc.Velocity);
			}

			// Gather particles from all Particle Systems
			std::vector<Particle> aggregatedParticles;
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();
				aggregatedParticles.insert(aggregatedParticles.end(), pc.Particles.begin(), pc.Particles.end());
			}

			Renderer::FillParticleBuffer(aggregatedParticles);
		}

		// Updated Meshes to check which LOD Group it should use during the rendering.
		{
			auto view = mRegistry.view<MeshComponent, TransformComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };

				MeshComponent& mc = e.GetComponent<MeshComponent>();
				TransformComponent& tc = e.GetComponent<TransformComponent>();

				if (mc.MeshObject->HasLODGroups())
				{
					double maxDistance = 10000.0;
					double distance = Vector3::Length(tc.Translation);
					double remappedDistance = std::clamp(distance / maxDistance, 0.0, 1.0);
					mc.MeshObject->UpdateLODDistance(remappedDistance);

					std::vector<float> thresholds = mc.MeshObject->GetLODThresholds();

					int activeLOD = 0; // Default to LOD0

					if (remappedDistance > thresholds[1])
						activeLOD = 2; // LOD2
					else if (remappedDistance > thresholds[0])
						activeLOD = 1; // LOD1

					mc.MeshObject->SetActiveLODGroup(activeLOD);
				}
			}
		}

		// Start a rebuild of the planet if needed
		{
			auto view = mRegistry.view<PlanetComponent, TransformComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };

				TerrainDetailComponent* tdc = nullptr;
				TerrainColliderComponent* tcc = nullptr;
				PlanetComponent& pc = e.GetComponent<PlanetComponent>();
				TransformComponent& tc = e.GetComponent<TransformComponent>();

				if(e.HasComponent<TerrainDetailComponent>())
					tdc = &e.GetComponent<TerrainDetailComponent>();

				if (e.HasComponent<TerrainColliderComponent>())
					tcc = &e.GetComponent<TerrainColliderComponent>();

				if (mainCamera)
				{
					if (pc.IsDirty)
					{
						PlanetSystem::GenerateDistanceLUT(pc.DistanceLUT, pc.PlanetData.radius, mainCameraComponent->Camera.GetPerspectiveVerticalFOV(), mViewportWidth);
						PlanetSystem::GenerateFaceDotLevelLUT(pc.FaceLevelDotLUT, tc.Scale.x, pc.PlanetData.maxAltitude);
						PlanetSystem::GenerateHeightMultLUT(pc.HeightMultLUT, tc.Scale.x, pc.PlanetData.maxAltitude);

						pc.IsDirty = false;
					}

					DirectX::XMVECTOR cameraForward = { 0.0f, 0.0f, 1.0f };
					DirectX::XMVECTOR cameraPos, cameraRot, cameraScale;

					DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, mainCameraTransform->GetTransform());
					cameraForward = DirectX::XMVector3Rotate(cameraForward, cameraRot);

					InvalidateFrustum();

					DirectX::XMMATRIX noScaleModelMatrix = DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(tc.RotationEulerAngles.x), DirectX::XMConvertToRadians(tc.RotationEulerAngles.y), DirectX::XMConvertToRadians(tc.RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.RotationQuaternion))
						* DirectX::XMMatrixTranslation(tc.Translation.x, tc.Translation.y, tc.Translation.z);

					// Starting new thread to create a new planet if one isn't already being created
					PlanetSystem::RegeneratePlanet(mFrustum, tc.Scale, tc.Translation, noScaleModelMatrix, cameraPos, mSettings.BackfaceCulling, mSettings.FrustumCulling, pc, tcc->BuildColliders, tcc->BuildColliderPositions, tdc);

					// Check if planet build is ready and if that is the case move it to the render mesh
					PlanetSystem::UpdatePlanet(pc.RenderMesh, pc.BuildVertices, pc.BuildIndices, *tcc);

					if (e.HasComponent<TerrainObjectComponent>()) 
					{
						TerrainObjectComponent& toc = e.GetComponent<TerrainObjectComponent>();

						PlanetSystem::DetailObjectPlacement(pc, toc, noScaleModelMatrix, cameraPos);
					}
				}
				else
					TOAST_CORE_ERROR("No primary camera present, unable to render the planet");
			}
		}

		DirectX::XMFLOAT4 cameraPosFloat;
		DirectX::XMStoreFloat4(&cameraPosFloat, editorCamera->GetPosition());

		// 3D Rendering
		Renderer::BeginScene(this, *editorCamera, cameraPosFloat);
		{
			// Skybox!
			{
				if (mEnvironment.RadianceMap)
					Renderer::SubmitSkybox(DirectX::XMFLOAT4(DirectX::XMVectorGetX(editorCamera->GetPosition()), DirectX::XMVectorGetY(editorCamera->GetPosition()), DirectX::XMVectorGetZ(editorCamera->GetPosition()), 0.0f), editorCamera->GetViewMatrix(), editorCamera->GetProjection(), mEnvironmentIntensity, mSkyboxLod);
			}

			// Meshes!
			auto viewMeshes = mRegistry.view<TransformComponent, MeshComponent>();
			for (auto entity : viewMeshes)
			{
				auto [transform, mesh] = viewMeshes.get<TransformComponent, MeshComponent>(entity);
				//Do not submit mesh if it's a planet
				//if (!mesh.MeshObject->GetIsPlanet())
				//{
					//Entity e = { entity, this };
					//auto& tc = e.GetComponent<TagComponent>();

					//if (mesh.MeshObject->GetSubmeshes().at(0).IndexCount == 234)
					//	TOAST_CORE_CRITICAL("MESH WITH 234 indices FOUND!!: %s, instanced: %d", tc.Tag.c_str(), mesh.MeshObject->IsInstanced());

					switch (mSettings.WireframeRendering)
					{
					case Settings::Wireframe::NO:
					{
						Renderer::SubmitMesh(mesh.MeshObject, transform.GetTransform(), (int)entity, false, 0);

						break;
					}
					case Settings::Wireframe::YES:
					{
						Renderer::SubmitMesh(mesh.MeshObject, transform.GetTransform(), (int)entity, true, 0);

						break;
					}
					case Settings::Wireframe::ONTOP:
					{
						// TODO

						break;
					}
					}
				//}

				if (mSelectedEntity == entity)
					Renderer::SubmitSelecetedMesh(mesh.MeshObject, transform.GetTransform());

				mStats.VerticesCount += static_cast<uint32_t>(mesh.MeshObject->GetVertices().size());
			}

			auto terrainObjectMeshes = mRegistry.view<TransformComponent, TerrainObjectComponent>();
			for (auto entity : terrainObjectMeshes)
			{
				auto [transform, terrainObject] = terrainObjectMeshes.get<TransformComponent, TerrainObjectComponent>(entity);
				if (terrainObject.MeshObject)
				{
					if (terrainObject.MeshObject->GetNumberOfInstances(0) > 0)
					{
						switch (mSettings.WireframeRendering)
						{
						case Settings::Wireframe::NO:
						{
							Renderer::SubmitMesh(terrainObject.MeshObject, transform.GetTransform(), (int)entity, false, 0);

							break;
						}
						case Settings::Wireframe::YES:
						{
							Renderer::SubmitMesh(terrainObject.MeshObject, transform.GetTransform(), (int)entity, true, 0);

							break;
						}
						}
					}
				}
			}

			// Planets!
			auto viewPlanets = mRegistry.view<TransformComponent, PlanetComponent>();
			for (auto entity : viewPlanets)
			{
				auto [transform, planet] = viewPlanets.get<TransformComponent, PlanetComponent>(entity);
				
				planet.PlanetData.planetCenter = transform.Translation;

				switch (mSettings.WireframeRendering)
				{
				case Settings::Wireframe::NO:
				{
					if (planet.RenderMesh->mLODGroups[0]->Submeshes.size() > 0)
						Renderer::SubmitMesh(planet.RenderMesh, DirectX::XMMatrixIdentity(), (int)entity, false, 1, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

					break;
				}
				case Settings::Wireframe::YES:
				{
					if (planet.RenderMesh->mLODGroups[0]->Submeshes.size() > 0)
						Renderer::SubmitMesh(planet.RenderMesh, DirectX::XMMatrixIdentity(), (int)entity, true, 1, &planet.PlanetData, planet.PlanetData.atmosphereToggle);

					break;
				}
				case Settings::Wireframe::ONTOP:
				{
					// TODO

					break;
				}
				}

				if (mSelectedEntity == entity)
					Renderer::SubmitSelecetedMesh(planet.RenderMesh, transform.GetTransform());

				mStats.VerticesCount += static_cast<uint32_t>(planet.RenderMesh->GetVertices().size());
			}

			Renderer::EndScene(true, mSettings.Shadows, mSettings.SSAO, mSettings.DynamicIBL, *editorCamera, cameraPosFloat);
		}

		// Debug Rendering
		RendererDebug::BeginScene(*editorCamera);
		{
			// Frustum
			if (mSettings.CameraFrustum && mFrustum)
				RendererDebug::SubmitCameraFrustum(mFrustum);

			// Sun light frustum
			if (mSettings.SunLightFrustum) 
			{
				DirectX::XMFLOAT3 sunLightFrustumColor = { 1.0f, 1.0f, 0.0f };

				RendererDebug::SubmitLine(frustumCorners[0], frustumCorners[1], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[1], frustumCorners[2], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[2], frustumCorners[3], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[3], frustumCorners[0], sunLightFrustumColor);

				RendererDebug::SubmitLine(frustumCorners[4], frustumCorners[5], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[5], frustumCorners[6], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[6], frustumCorners[7], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[7], frustumCorners[4], sunLightFrustumColor);

				// Connecting edges between near and far planes
				RendererDebug::SubmitLine(frustumCorners[0], frustumCorners[4], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[1], frustumCorners[5], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[2], frustumCorners[6], sunLightFrustumColor);
				RendererDebug::SubmitLine(frustumCorners[3], frustumCorners[7], sunLightFrustumColor);
			}

			// Colliders
			auto entities = mRegistry.view<TransformComponent>();
			for (auto entity : entities)
			{
				DirectX::XMVECTOR pos = { 0.0f, 0.0f, 0.0f }, rot = { 0.0f, 0.0f, 0.0f }, scale = { 0.0f, 0.0f, 0.0f };

				Entity e{ entity, this };

				auto tc = e.GetComponent<TransformComponent>();
				DirectX::XMMatrixDecompose(&scale, &rot, &pos, tc.GetTransform());

				bool hasSphereCollider = e.HasComponent<SphereColliderComponent>();
				bool hasBoxCollider = e.HasComponent<BoxColliderComponent>();

				Ref<Mesh> colliderMesh;
				bool renderCollider = false;

				if (hasSphereCollider)
				{
					auto scc = e.GetComponent<SphereColliderComponent>();
					scale = { (float)scc.Collider->mRadius, (float)scc.Collider->mRadius , (float)scc.Collider->mRadius };
					colliderMesh = scc.ColliderMesh;
					renderCollider = scc.RenderCollider;
				}
				else if (hasBoxCollider)
				{
					auto bcc = e.GetComponent<BoxColliderComponent>();
					scale = { (float)bcc.Collider->mSize.x, (float)bcc.Collider->mSize.y, (float)bcc.Collider->mSize.z };
					colliderMesh = bcc.ColliderMesh;
					renderCollider = bcc.RenderCollider;
				}

				DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScalingFromVector(scale) * DirectX::XMMatrixRotationQuaternion(rot) * DirectX::XMMatrixTranslationFromVector(pos);

				if(renderCollider && mSettings.RenderColliders)
					RendererDebug::SubmitMesh(colliderMesh, transform);
			}

			// Particles Cubes, this is to guide the user to easier see where the particles will spawn
			auto viewEntitiesParticles = mRegistry.view<TransformComponent, ParticlesComponent>();
			for (auto entity : viewEntitiesParticles)
			{
				Entity e{ entity, this };

				DirectX::XMMATRIX transform = e.GetComponent<TransformComponent>().GetTransform();
				auto pc = e.GetComponent<ParticlesComponent>();

				if (e.HasParent())
				{
					Entity parent = FindEntityByUUID(e.GetParentUUID());

					DirectX::XMMATRIX& parentTransform = parent.GetComponent<TransformComponent>().GetTransform();
					transform = DirectX::XMMatrixMultiply(transform, parentTransform);
				}

				RendererDebug::SubmitMesh(pc.GuideMesh, transform, false);
			}
		}

		RendererDebug::EndScene(true, false, mSettings.RenderUI, mSettings.Grid);

		// 2D UI Rendering
		if (mSettings.RenderUI) 
		{
			Renderer2D::BeginScene(*editorCamera);
			{
				DirectX::XMFLOAT3 finalPosition;

				//Panels
				auto uiPanelEntites = mRegistry.view<TransformComponent, UIPanelComponent>();

				for (auto entity : uiPanelEntites)
				{
					auto [tc, upc] = uiPanelEntites.get<TransformComponent, UIPanelComponent>(entity);

					Entity e{ entity, this };

					const auto& name = e.GetComponent<TagComponent>().Tag;
					
					if (upc.Panel->GetVisible())
					{
						if (e.HasParent())
						{
							Entity parent = FindEntityByUUID(e.GetParentUUID());
							bool is2DParent = parent.HasComponent<UIPanelComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UITextComponent>();

							auto& parentTC = parent.GetComponent<TransformComponent>();

							DirectX::XMVECTOR parentWorldPos = XMLoadFloat3(&parentTC.Translation);
							DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&editorCamera->GetViewMatrix());
							DirectX::XMMATRIX projectionMatrix = DirectX::XMLoadFloat4x4(&editorCamera->GetProjection());

							DirectX::XMFLOAT3 parentScreenPos;

							DirectX::XMVECTOR parentViewPos = DirectX::XMVector3Transform(parentWorldPos, viewMatrix);
							DirectX::XMVECTOR clipSpacePos = DirectX::XMVector3Transform(parentViewPos, projectionMatrix);

							float x = clipSpacePos.m128_f32[0];
							float y = clipSpacePos.m128_f32[1];
							float z = clipSpacePos.m128_f32[2];
							float w = clipSpacePos.m128_f32[3];

							float ndcX = x / w;
							float ndcY = y / w;
							float ndcZ = z / w;

							float screenX = (ndcX * 0.5f + 0.5f) * mViewportWidth;
							float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * mViewportHeight;

							parentScreenPos.x = screenX - (mViewportWidth / 2.0f);
							parentScreenPos.y = -(screenY - (mViewportHeight / 2.0f));
							parentScreenPos.z = ndcZ;

							if (is2DParent)
							{
								DirectX::XMFLOAT3 parentPosition = FindEntityByUUID(e.GetParentUUID()).GetComponent<TransformComponent>().Translation;
								DirectX::XMFLOAT3 position = tc.Translation;
								finalPosition = { position.x + parentPosition.x, position.y + parentPosition.y, 1.0f };
							}
							else
								finalPosition = { tc.Translation.x , tc.Translation.y, 1.0f };

							if (upc.Panel->GetConnectToParent()) 
								Renderer2D::SubmitConnector(tc.Translation, tc.Scale, *upc.Panel->GetCornerRadius(), parentScreenPos, 3.0f, *upc.Panel->GetBorderSize());
						}
						else
							finalPosition = { tc.Translation.x , tc.Translation.y, 1.0f };

						std::string panelTextureName;
						if (!upc.Panel->GetUseColor())
							panelTextureName = upc.Panel->GetTextureFilepath();

						Renderer2D::SubmitPanel(finalPosition, { tc.Scale.x, tc.Scale.y, *upc.Panel->GetCornerRadius(), *upc.Panel->GetBorderSize() }, upc.Panel->GetColorF4(), (int)entity, !upc.Panel->GetUseColor(), panelTextureName, false);
					}
				}

				//Buttons
				auto uiButtonEntites = mRegistry.view<TransformComponent, UIButtonComponent>();

				for (auto entity : uiButtonEntites)
				{
					auto [tc, ubc] = uiButtonEntites.get<TransformComponent, UIButtonComponent>(entity);

					Entity e{ entity, this };

					bool renderButton = true;

					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());

						if (parent.HasComponent<UIPanelComponent>())
						{
							UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();

							renderButton = parentPanel.Panel->GetVisible();
						}

						DirectX::XMFLOAT3 parentPosition = FindEntityByUUID(e.GetParentUUID()).GetComponent<TransformComponent>().Translation;
						DirectX::XMFLOAT3 position = tc.Translation;
						finalPosition = { position.x + parentPosition.x, position.y + parentPosition.y, 1.0f };
					}
					else
						finalPosition = { tc.Translation.x , tc.Translation.y, 1.0f };

					if(renderButton)
						Renderer2D::SubmitButton(finalPosition, { tc.Scale.x, tc.Scale.y, *ubc.Button->GetCornerRadius(), 1.0f }, ubc.Button->GetColorF4(), (int)entity, !ubc.Button->GetUseColor(), false);
				}

				//Texts
				auto uiTextEntites = mRegistry.view<TransformComponent, UITextComponent>();

				for (auto entity : uiTextEntites)
				{
					auto [tc, uitc] = uiTextEntites.get<TransformComponent, UITextComponent>(entity);

					Entity e{ entity, this };

					bool renderText = true;

					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());

						if (parent.HasComponent<UIPanelComponent>())
						{
							UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();

							renderText = parentPanel.Panel->GetVisible();
						}

						DirectX::XMFLOAT3 parentPosition = FindEntityByUUID(e.GetParentUUID()).GetComponent<TransformComponent>().Translation;
						DirectX::XMFLOAT3 position = tc.Translation;
						finalPosition = { position.x + parentPosition.x, position.y + parentPosition.y, 2.0f };
					}
					else
						finalPosition = { tc.Translation.x , tc.Translation.y, 2.0f };

					if (renderText)
						Renderer2D::SubmitText(finalPosition, { tc.Scale.x, tc.Scale.y, 1.0f, 1.0f }, uitc.Text, (int)entity, true);
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

	void Scene::InvalidateFrustum()
	{
		Matrix planetTransform;

		auto view = mRegistry.view<TransformComponent, CameraComponent>();
		for (auto entity : view)
		{
			auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

			if (camera.Primary)
			{
				auto planetView = mRegistry.view<TransformComponent, PlanetComponent>();
				for (auto pEntity : planetView)
				{
					auto [pTransform, planet] = planetView.get<TransformComponent, PlanetComponent>(pEntity);

					DirectX::XMMATRIX noScalePlanetMatrix = DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(pTransform.RotationEulerAngles.x), DirectX::XMConvertToRadians(pTransform.RotationEulerAngles.y), DirectX::XMConvertToRadians(pTransform.RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&pTransform.RotationQuaternion))
						* DirectX::XMMatrixTranslation(pTransform.Translation.x, pTransform.Translation.y, pTransform.Translation.z);

					planetTransform = { noScalePlanetMatrix };

					mInvalidatePlanet = true;
				}

				Vector3 cameraTranslation = { transform.Translation };
				Matrix cameraTransform = { transform.GetTransform() };

				mFrustum->Invalidate(camera.Camera.GetAspecRatio(), camera.Camera.GetPerspectiveVerticalFOV(), camera.Camera.GetNearClip(), camera.Camera.GetFarClip());
				mFrustum->Update(cameraTransform, planetTransform);
			}
		}
	}

	Entity Scene::FindEntityByName(std::string_view name)
	{
		auto view = mRegistry.view<TagComponent>();
		for (auto entity : view)
		{
			const auto& canditate = view.get<TagComponent>(entity).Tag;
			if (canditate == name)
				return Entity{ entity, this };
		}

		return Entity{};
	}

	Entity Scene::FindChildEntityByName(std::string_view parentName, std::string_view childName)
	{
		Entity parentEntity = FindEntityByName(parentName);
		if (!parentEntity)
			return Entity{};

		if (!parentEntity.HasComponent<RelationshipComponent>())
			return Entity{};

		const RelationshipComponent& relationship = parentEntity.GetComponent<RelationshipComponent>();

		for (const UUID& childUUID : relationship.Children)
		{
			Entity childEntity = FindEntityByUUID(childUUID);
			if (!childEntity)
				continue;

			if (childEntity.HasComponent<TagComponent>())
			{
				const TagComponent& tagComp = childEntity.GetComponent<TagComponent>();
				if (tagComp.Tag == childName)
					return childEntity;
			}
		}

		return Entity{};
	}

	Entity Scene::FindEntityByUUID(UUID uuid)
	{
		TOAST_PROFILE_FUNCTION();

		if (mEntityIDMap.find(uuid) != mEntityIDMap.end())
			return Entity{ mEntityIDMap.at(uuid), this};

		return {};
	}

	void Scene::AddChildEntity(Entity entity, Entity parent)
	{
		entity.SetParentUUID(parent.GetUUID());
		parent.Children().push_back(entity.GetUUID());
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
		// Settings
		target->mSettings.PhysicSlowmotion = mSettings.PhysicSlowmotion;

		// Environment
		target->mLightEnvironment = mLightEnvironment;

		target->mEnvironment = mEnvironment;
		target->mSkyboxTexture = mSkyboxTexture;
		target->mSkyboxLod = mSkyboxLod;

		//Collider
		target->mCubeColliderMaterial = mCubeColliderMaterial;
		target->mSphereColliderMaterial = mSphereColliderMaterial;

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

		CopyComponent<RelationshipComponent>(target->mRegistry, mRegistry, enttMap);
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
		CopyComponent<BoxColliderComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<TerrainColliderComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<UIPanelComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<UITextComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<UIButtonComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<TerrainDetailComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<TerrainObjectComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<ParticlesComponent>(target->mRegistry, mRegistry, enttMap);
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
		TerrainDetailComponent* tdc = nullptr;

		Ref<Material> planetMaterial = MaterialLibrary::Get("Planet");

		component.RenderMesh = CreateRef<Mesh>(planetMaterial);
		component.RenderMesh->mTopology = PrimitiveTopology::TRIANGLELIST;

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
				TOAST_CORE_INFO("To add a planet a camera must be present");
				return;
			}
		}

		tc = entity.GetComponent<TransformComponent>();
		if(entity.HasComponent<TerrainDetailComponent>())
			tdc = &entity.GetComponent<TerrainDetailComponent>();

		DirectX::XMVECTOR cameraPos, cameraRot, cameraScale, cameraForward;

		cameraForward = { 0.0f, 0.0f, 1.0f };
		DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);
		cameraForward = DirectX::XMVector3Rotate(cameraForward, cameraRot);

		InvalidateFrustum();

		PlanetSystem::GenerateDistanceLUT(component.DistanceLUT, component.PlanetData.radius, mainCamera->GetPerspectiveVerticalFOV(), mViewportWidth);
		PlanetSystem::GenerateHeightMultLUT(component.HeightMultLUT, component.PlanetData.radius, component.PlanetData.maxAltitude);
		PlanetSystem::GenerateFaceDotLevelLUT(component.FaceLevelDotLUT, tc.Scale.x, component.PlanetData.maxAltitude);

		DirectX::XMMATRIX noScaleModelMatrix = DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(tc.RotationEulerAngles.x), DirectX::XMConvertToRadians(tc.RotationEulerAngles.y), DirectX::XMConvertToRadians(tc.RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.RotationQuaternion))
			* DirectX::XMMatrixTranslation(tc.Translation.x, tc.Translation.y, tc.Translation.z);
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		auto tc = entity.GetComponent<TransformComponent>();
		Vector3 cameraTranslation = { tc.Translation };

		component.Camera.SetViewportSize(mViewportWidth, mViewportHeight);

		mFrustum = CreateRef<Frustum>();
		mFrustum->Invalidate(component.Camera.GetAspecRatio(), component.Camera.GetPerspectiveVerticalFOV(), component.Camera.GetNearClip(), component.Camera.GetFarClip());
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

	template<>
	void Scene::OnComponentAdded<RigidBodyComponent>(Entity entity, RigidBodyComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SphereColliderComponent>(Entity entity, SphereColliderComponent& component)
	{
		component.Collider = CreateRef<ShapeSphere>(1.0f);

		component.ColliderMesh = CreateRef<Mesh>("..\\Toaster\\assets\\meshes\\Sphere.gltf", Vector3(0.0, 0.0, 1.0));
	}

	template<>
	void Scene::OnComponentAdded<BoxColliderComponent>(Entity entity, BoxColliderComponent& component)
	{
		component.Collider = CreateRef<ShapeBox>(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));

		component.ColliderMesh = MeshFactory::CreateCube(1.0f, { 0.0, 0.0, 1.0 });
	}

	template<>
	void Scene::OnComponentAdded<TerrainColliderComponent>(Entity entity, TerrainColliderComponent& component)
	{
		component.Collider = CreateRef<ShapeTerrain>();
	}

	template<>
	void Scene::OnComponentAdded<UIPanelComponent>(Entity entity, UIPanelComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<UITextComponent>(Entity entity, UITextComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<UIButtonComponent>(Entity entity, UIButtonComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<TerrainDetailComponent>(Entity entity, TerrainDetailComponent& component)
	{
		if(component.Seed == 0)
			component.Seed = Math::GenerateRandomSeed();
	}

	template<>
	void Scene::OnComponentAdded<TerrainObjectComponent>(Entity entity, TerrainObjectComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<ParticlesComponent>(Entity entity, ParticlesComponent& component)
	{
		component.GuideMesh = MeshFactory::CreateCube(1.0f, { 1.0, 0.0, 0.0 });
	}

}