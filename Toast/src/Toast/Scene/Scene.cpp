#include "tpch.h"
#include "Scene.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"
#include "Toast/Scene/MovementSystem.h"
#include "Toast/Scene/Prefab.h"
#include "Toast/Scene/SelectionSystem.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"
#include "Toast/Renderer/MeshFactory.h"
#include "Toast/Renderer/PlanetSystem.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"

namespace Toast {

	// TODO MOVE THIS TO RENDERER2D
	inline DirectX::XMFLOAT3 UIToCenteredRenderSpace(const DirectX::XMFLOAT3& uiPosTL, float viewportW, float viewportH)
	{
		DirectX::XMFLOAT3 out;
		out.x = uiPosTL.x - viewportW * 0.5f;
		out.y = (viewportH * 0.5f) - uiPosTL.y; // flip Y so UI down becomes render up
		out.z = uiPosTL.z;
		return out;
	}

	struct SceneComponent
	{
		UUID SceneID;
	};

	Scene::Scene(std::string name)
		: mName(name), mSceneID(UUID())
	{
		mSceneEntity = mRegistry.create();
		mRegistry.emplace<SceneComponent>(mSceneEntity, mSceneID);

		mPlanet = CreateRef<Planet>();
		mPlanet->Initialize();

		mPhysicsEngine = CreateRef<PhysicsEngine>();
		mPhysicsEngine->Initialize(this);

		mSelectionSystem = CreateScope<SelectionSystem>(this);
		mMovementSystem = CreateScope<MovementSystem>(this);
	}

	Scene::~Scene()
	{
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
				const auto& sc = e.GetComponent<ScriptComponent>();
				ScriptEngine::OnCreateEntityWithClass(e, sc.ClassName);
			}

			// Instantiate all scene script entities
			auto sceneScriptView = mRegistry.view<SceneScriptComponent>();
			for (auto entity : sceneScriptView)
			{
				Entity e = { entity, this };
				const auto& ssc = e.GetComponent<SceneScriptComponent>();
				ScriptEngine::OnCreateEntityWithClass(e, ssc.ClassName);
			}
		}

		// Reseting particle system to make sure nothing is carried over between play and edit state
		Renderer::GetParticleSystem()->Reset();

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

		// Reseting particle system to make sure nothing is carried over between play and edit state
		Renderer::GetParticleSystem()->Reset();
	}

	void Scene::OnEvent(Event& e)
	{
		TOAST_PROFILE_FUNCTION();

		EventDispatcher dispatcher(e);

		if (mIsRunning && !mRuntimeBlocked)
		{
			dispatcher.Dispatch<MouseButtonPressedEvent>(TOAST_BIND_EVENT_FN(Scene::OnMouseButtonPressed));
			dispatcher.Dispatch<MouseButtonReleasedEvent>(TOAST_BIND_EVENT_FN(Scene::OnMouseButtonReleased));
		}

		dispatcher.Dispatch<MouseMovedEvent>(TOAST_BIND_EVENT_FN(Scene::OnMouseMoved));
	}

	bool Scene::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{	
		// Check that a valid entity is being hovered over by the mouse
		if (mHoveredEntity != entt::null)
		{
			Entity entity = { mHoveredEntity, this };
			UUID uuid = entity.GetComponent<IDComponent>().ID;
			std::string tag = entity.GetComponent<TagComponent>().Tag;
			if (entity.HasComponent<ScriptComponent>() && !entity.HasComponent<UIButtonComponent>())
				ScriptEngine::OnEventEntity(entity);
			if (entity.HasComponent<UIButtonComponent>())
				entity.GetComponent<UIButtonComponent>().IsClicked = true;
		}

		auto view = mRegistry.view<SceneScriptComponent>();
		for (auto entity : view)
		{
			Entity e = { entity, this };
			ScriptEngine::OnEventEntity(e);
		}

		return true;
	}

	bool Scene::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
	{
		auto buttonView = mRegistry.view<UIButtonComponent, IDComponent>();

		if (mHoveredEntity != entt::null)
		{
			Entity entity = { mHoveredEntity, this };

			if (entity.HasComponent<ScriptComponent>() && entity.HasComponent<UIButtonComponent>())
				ScriptEngine::OnEventEntity(entity);
		}

		for (auto buttonEntity : buttonView)
		{
			Entity e = { buttonEntity, this };
			e.GetComponent<UIButtonComponent>().IsClicked = false;
		}

		return true;
	}

	bool Scene::OnMouseMoved(MouseMovedEvent& e)
	{
		TOAST_PROFILE_FUNCTION();

		mMouseX = e.GetX();
		mMouseY = e.GetY();

		return false;
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

		DirectX::XMMATRIX cameraTransform;
		{
			auto view = mRegistry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				if (camera.Primary)
				{
					mMainCamera = &camera.Camera;

					if (Renderer::GetFinalRT()->GetSize() != std::make_tuple(mMainCamera->GetOrthographicWidth(), mMainCamera->GetOrthographicHeight()))
					{
						auto [width, height] = Renderer::GetFinalRT()->GetSize();
						mMainCamera->SetOrthographicSize(width, height);
					}

					cameraTransform = transform.GetTransform();

					DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);

					break;
				}
				// if no camera is present nothing is rendered
				else
					return;

				if(camera.IsDirty)
					mPlanet->GetIcosphereMesh()->SetDistanceLUTDirty();
			}
		}

		if (!mIsPaused && !mRuntimeBlocked)
		{
			// Updating box colliders
			{
				auto view = mRegistry.view<BoxColliderComponent, TransformComponent>();
				for (auto entity : view)
				{
					auto [bcc, tc] = view.get<BoxColliderComponent, TransformComponent>(entity);
					DirectX::XMVECTOR totalRotVec = DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&tc.RotationQuaternion), DirectX::XMQuaternionRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&tc.RotationEulerAngles)));
					DirectX::XMFLOAT4 totalRot;
					DirectX::XMStoreFloat4(&totalRot, totalRotVec);
				}
			}

			float deltaTime = ts.GetSeconds() * mTimeScale;

			mPhysicsEngine->Update(deltaTime);

			// Scripting
			{
				// C# Entity OnUpdate
				auto view = mRegistry.view<ScriptComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };
					ScriptEngine::OnUpdateEntity(e, ts * mTimeScale);
				}

				// C# Scene Script OnUpdate
				auto sceneScriptView = mRegistry.view<SceneScriptComponent>();
				for (auto entity : sceneScriptView)
				{
					Entity e = { entity, this };
					ScriptEngine::OnUpdateEntity(e, ts * mTimeScale);
				}
			}
		}

		// Process lights
		{
			DirectX::XMFLOAT4 direction = { 0.0f, 0.0f, 0.0f, 0.0f };
			DirectX::XMVECTOR sunLightDirWS = XMVectorZero();

			mLightEnvironment = LightEnvironment();
			auto lights = mRegistry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
			uint32_t directionalLightIndex = 0;

			const float camNear = mMainCamera->GetNearClip();
			const float camFar = mMainCamera->GetFarClip();

			const float shadowFar = std::min(camFar, mSettings.Shadows.ShadowDistance);

			const float fovY = Math::DegreesToRadians(mMainCamera->GetVerticalFOV());
			const float aspect = mMainCamera->GetAspectRatio();

			float cascadeEnds[MaxCascades] = {};

			const bool shadowsEnabled = mSettings.Shadows.Active;

			if (shadowsEnabled)
			{
				mSettings.Shadows.CascadeCount = std::clamp(mSettings.Shadows.CascadeCount, 1u, MaxCascades);
				mSettings.Shadows.Lambda = std::clamp(mSettings.Shadows.Lambda, 0.0f, 1.0f);

				const float farForShadows = std::max(shadowFar, camNear + 1.0f);


				Renderer::ComputeCascadeEnds(camNear, farForShadows, mSettings.Shadows.CascadeCount, mSettings.Shadows.Lambda, cascadeEnds);

				mSettings.Shadows.IsDirty = false;
			}

			for (auto entity : lights)
			{
				auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);

				DirectX::XMMATRIX transform = transformComponent.GetTransform();

				// Extract forward (Z axis) -> lightDir (same as your code)
				DirectX::XMVECTOR lightDir = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(transform.r[2]));
				sunLightDirWS = lightDir;

				DirectX::XMStoreFloat4(&direction, lightDir);
				direction.w = 0.0f;

				DirectX::XMFLOAT4 radiance(lightComponent.Radiance.x, lightComponent.Radiance.y, lightComponent.Radiance.z, 0.0f);

				// Fill light entry
				auto& out = mLightEnvironment.DirectionalLights[directionalLightIndex++];

				out.Direction = direction;
				out.Radiance = radiance;
				out.Multiplier = lightComponent.Intensity;

				out.CascadeCount = 0;
				out.ShadowDistance = 0.0f;
				std::fill(std::begin(out.CascadeEnds), std::end(out.CascadeEnds), 0.0f);

				if (shadowsEnabled)
				{
					// Store split info on the sun light
					out.CascadeCount = mSettings.Shadows.CascadeCount;
					out.ShadowDistance = shadowFar;
					out.ConstantBias = mSettings.Shadows.ConstantBias;
					out.SlopeBias = mSettings.Shadows.SlopeScaledBias;
					memcpy(out.CascadeEnds, cascadeEnds, sizeof(float) * MaxCascades);

					Renderer::ComputeCSMLightViewProj({ 0.0f, 0.0f, 0.0f}, mMainCamera, { cameraRot }, fovY, aspect, sunLightDirWS, camNear, out.CascadeEnds, out.CascadeCount, out.ShadowDistance, out.LightViewProj);
				}
				else
				{
					// Fill identity to keep shaders safe
					for (uint32_t i = 0; i < MaxCascades; ++i)
						out.LightViewProj[i] = DirectX::XMMatrixIdentity();
				}
			}

			mEnvironment.SunUV = ComputeSunUVFromDirection(DirectX::XMFLOAT3(direction.x, direction.y, direction.z), DirectX::XMLoadFloat4x4(&mMainCamera->GetViewMatrix()), DirectX::XMLoadFloat4x4(&mMainCamera->GetProjection()));
		}

		// Process Skeletal/Blender Animations
		auto view = mRegistry.view<TransformComponent, MeshComponent>();
		for (auto entity : view)
		{
			auto [tc, mesh] = view.get<TransformComponent, MeshComponent>(entity);

			if (mesh.MeshObject->GetIsAnimated())
				mesh.MeshObject->OnUpdate(ts * mTimeScale);
		}

		// Process Transform Interpolation (rotation, etc.), Engine side animation
		auto transformView = mRegistry.view<TransformComponent>();
		for (auto entity : transformView)
		{
			auto& tc = transformView.get<TransformComponent>(entity);

			// Rotation
			if (tc.IsRotating && tc.AngularSpeed > 0.0f)
			{
				DirectX::XMVECTOR currentRot = tc.GetTotalRotationQuaternion();
				DirectX::XMVECTOR targetRot = DirectX::XMLoadFloat4(&tc.TargetRotationQuaternion);

				float dot = std::abs(DirectX::XMVectorGetX(DirectX::XMVector4Dot(currentRot, targetRot)));
				dot = (std::min)(dot, 1.0f);
				float remainingRad = 2.0f * std::acos(dot);

				if (remainingRad < 0.001f)
				{
					// Snap to target and stop rotating
					DirectX::XMStoreFloat4(&tc.RotationQuaternion, targetRot);
					tc.RotationEulerAngles = { 0.0f, 0.0f, 0.0f };
					tc.IsRotating = false;
					tc.IsDirty = true;
					continue;
				}

				float stepRad = DirectX::XMConvertToRadians(tc.AngularSpeed) * ts * mTimeScale;
				float t = (std::min)(stepRad / remainingRad, 1.0f);

				DirectX::XMVECTOR newRot = DirectX::XMQuaternionSlerp(currentRot, targetRot, t);
				newRot = DirectX::XMQuaternionNormalize(newRot);

				DirectX::XMStoreFloat4(&tc.RotationQuaternion, newRot);
				tc.RotationEulerAngles = { 0.0f, 0.0f, 0.0f }; // Reset Euler angles to avoid confusion, we only use the quaternion for rotation when IsRotating is true
				tc.IsDirty = true;
			}

			// Translation
			if (tc.IsTranslating && tc.TranslationSpeed > 0.0f)
			{
				DirectX::XMVECTOR current = DirectX::XMLoadFloat3(&tc.Translation);
				DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&tc.TargetTranslation);
				DirectX::XMVECTOR toTarget = DirectX::XMVectorSubtract(target, current);
				float remaining = DirectX::XMVectorGetX(DirectX::XMVector3Length(toTarget));

				float step = tc.TranslationSpeed * ts * mTimeScale;
				if (step >= remaining || remaining < 0.0001f)
				{
					DirectX::XMStoreFloat3(&tc.Translation, target);
					tc.IsTranslating = false;
					tc.IsDirty = true;
				}
				else
				{
					DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(toTarget);
					DirectX::XMVECTOR newPos = DirectX::XMVectorAdd(current, DirectX::XMVectorScale(dir, step));
					DirectX::XMStoreFloat3(&tc.Translation, newPos);
					tc.IsDirty = true;
				}
			}
		}

		if (mMainCamera)
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
						double distance = Vector3::Length(Vector3(tc.Translation) + Vector3(mMainCamera->GetWorldTranslation()));
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
				auto particleSystem = Renderer::GetParticleSystem();

				std::vector<EmitterParamsGPU> emitterParams;
				std::vector<uint32_t> emitCounts;

				auto view = mRegistry.view<ParticlesComponent>();
				for (auto entity : view)
				{
					if (emitterParams.size() >= MAX_EMITTERS)
					{
						TOAST_CORE_WARN("More than %d particle emitters - ignoring the rest.", MAX_EMITTERS);
						break;
					}

					Entity e = { entity, this };
					ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();
					TransformComponent& tc = e.GetComponent<TransformComponent>();

					// Reset previous spawn position so re-enabling it doesn't smear particles where they shouldn't be
					if (!pc.Emitting)
						pc.HasPrevSpawnPosition = false;

					DirectX::XMFLOAT3 spawnPosition = e.GetComponent<TransformComponent>().Translation;
					DirectX::XMMATRIX rotationMatrix = tc.GetRotation();

					// Inherited world-space velocity from a parent rigid body, if any.
					DirectX::XMFLOAT3 parentVelocity = { 0.0f, 0.0f, 0.0f };

					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());
						TransformComponent& parentTC = parent.GetComponent<TransformComponent>();

						DirectX::XMMATRIX parentTransform = parentTC.GetTransformWithoutScale();
						DirectX::XMVECTOR localPos = DirectX::XMLoadFloat3(&spawnPosition);
						DirectX::XMVECTOR worldPos = DirectX::XMVector3Transform(localPos, parentTransform);
						DirectX::XMStoreFloat3(&spawnPosition, worldPos);

						// First frame there is no previous position so we set prev spawn position to spawn position
						if (!pc.HasPrevSpawnPosition)
						{
							pc.PrevSpawnPosition = spawnPosition;
							pc.HasPrevSpawnPosition = true;
						}

						rotationMatrix = DirectX::XMMatrixMultiply(rotationMatrix, parentTC.GetRotation());

						// Velocity inheritance: only if the parent has a rigid body.
						// Restores the behavior dropped during the Step 2 test - this
						// is what stops a fast ship overtaking its own exhaust.
						if (parent.HasComponent<RigidBodyComponent>())
						{
							auto& parentRB = parent.GetComponent<RigidBodyComponent>();
							parentVelocity = {
								(float)parentRB.LinearVelocity.x,
								(float)parentRB.LinearVelocity.y,
								(float)parentRB.LinearVelocity.z
							};
						}
					}

					DirectX::XMFLOAT3 finalVelocity;
					{
						DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pc.Velocity);
						v = DirectX::XMVector3Transform(v, rotationMatrix);
						DirectX::XMStoreFloat3(&finalVelocity, v);
					}

					finalVelocity.x += parentVelocity.x;
					finalVelocity.y += parentVelocity.y;
					finalVelocity.z += parentVelocity.z;

					Renderer::SetParticleMaskTexture(pc.MaskTextureHandle);

					EmitterParamsGPU p = {};
					p.SpawnPosition = spawnPosition;
					p.PrevSpawnPosition = pc.PrevSpawnPosition;
					p.SpawnSize = tc.Scale;
					p.Velocity = finalVelocity;
					p.ConeAngleDegrees = pc.ConeAngleDegrees;
					p.BiasExponent = pc.BiasExponent;
					p.StartColor = pc.StartColor;
					p.EndColor = pc.EndColor;
					p.ColorBlendFactor = pc.ColorBlendFactor;
					p.MaxLifeTime = pc.MaxLifeTime;
					p.Size = pc.Size;
					p.GrowRate = pc.GrowRate;
					p.BurstInitial = pc.BurstInitial;
					p.BurstDecay = pc.BurstDecay;
					p.EmitFunction = static_cast<uint32_t>(pc.SpawnFunction);

					emitterParams.push_back(p);
					emitCounts.push_back(ParticleSystem::ComputeEmitCount(pc, ts));

					// Setting up for next frame
					pc.PrevSpawnPosition = spawnPosition;
				}

				particleSystem->OnUpdate(ts, emitterParams, emitCounts);
			} // End particle system

			DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, cameraTransform);
			DirectX::XMFLOAT4 cameraPosFloat;
			DirectX::XMStoreFloat4(&cameraPosFloat, cameraPos);

			DirectX::XMFLOAT4X4 fView, fInvView;
			DirectX::XMStoreFloat4x4(&fView, DirectX::XMMatrixInverse(nullptr, cameraTransform));
			DirectX::XMStoreFloat4x4(&fInvView, cameraTransform);
			mMainCamera->SetViewMatrix(fView);
			mMainCamera->SetInvViewMatrix(fInvView);

			// Movement System
			{
				mMovementSystem->OnUpdate(ts * mTimeScale);
			}

			// Start a rebuild of the planet if needed
			{
				if (mMainCamera)
				{
					InvalidateFrustum();

					mPlanet->OnUpdate(mMainCamera, cameraRot, { cameraPos }, { cameraPos }, mMainCamera->GetWorldTranslation(), cameraTransform, mPhysicsEngine.get(), mFrustum.get(), mMainCamera->GetViewMatrix(), mSettings.FrustumCullingMargin);
				}
			}

			// 3D Rendering
			Renderer::BeginScene(this, *mMainCamera, cameraPosFloat, mEnvironment, static_cast<int>(mSettings.WireframeRendering));
			{
				// Planet
				Renderer::SubmitPlanet(mPlanet, static_cast<int>(mSettings.WireframeRendering));

				// --- HACK (until part/submesh rework): cache animated part world-transforms so child
				// entities (e.g. rover on the elevator) can follow the animated parent. Fill BEFORE the
				// main loop so children read a complete map regardless of iteration order. ---
				std::unordered_map<UUID, DirectX::XMMATRIX> animatedPartTransforms;
				{
					auto prepassView = mRegistry.view<TransformComponent, MeshComponent>();
					for (auto entity : prepassView)
					{
						auto [transform, mesh] = prepassView.get<TransformComponent, MeshComponent>(entity);
						if (mesh.MeshObject->GetFilePath() == "")
							continue;

						// Compose this mesh's own world transform up the relationship chain.
						DirectX::XMMATRIX meshWorld = transform.GetTransform();
						{
							Entity current{ entity, this };
							while (current.HasComponent<RelationshipComponent>())
							{
								UUID parentUUID = current.GetComponent<RelationshipComponent>().ParentHandle;
								if (parentUUID == 0) break;
								Entity parentEntity = FindEntityByUUID(parentUUID);
								if (!parentEntity) break;
								meshWorld = DirectX::XMMatrixMultiply(meshWorld, parentEntity.GetComponent<TransformComponent>().GetTransform());
								current = parentEntity;
							}
						}

						auto& submeshes = mesh.MeshObject->mLODGroups[mesh.MeshObject->mActiveLODGroup]->Submeshes;
						for (auto& submesh : submeshes)
						{
							if (submesh.PartIndex >= mesh.MeshObject->mPartsUpdated.size())
								continue;
							const MeshPart& part = mesh.MeshObject->mPartsUpdated[submesh.PartIndex];
							if (part.EntityID == 0)
								continue;
							Entity partEntity = FindEntityByUUID(part.EntityID);
							if (!partEntity)
								continue;

							// Same composition as the render loop: submesh.Transform * partRest * meshWorld
							auto& partTransform = partEntity.GetComponent<TransformComponent>();
							DirectX::XMMATRIX animated = DirectX::XMMatrixMultiply(submesh.Transform, partTransform.GetTransform());
							animatedPartTransforms[part.EntityID] = DirectX::XMMatrixMultiply(animated, meshWorld);
						}
					}
				}

				// Meshes!
				auto viewMeshes = mRegistry.view<TransformComponent, MeshComponent>();
				for (auto entity : viewMeshes)
				{
					auto [transform, mesh] = viewMeshes.get<TransformComponent, MeshComponent>(entity);

					bool validMesh = mesh.MeshObject->GetFilePath() != "";

					if (!validMesh)
						continue;

					// Walk up the relationship tree to get parent transform included.
					DirectX::XMMATRIX worldTransform = transform.GetTransform();
					{
						Entity current{ entity, this };
						while (current.HasComponent<RelationshipComponent>())
						{
							UUID parentUUID = current.GetComponent<RelationshipComponent>().ParentHandle;
							if (parentUUID == 0) 
								break;

							// If the parent is an animated part, its cached transform is already full world space.
							auto it = animatedPartTransforms.find(parentUUID);
							if (it != animatedPartTransforms.end())
							{
								worldTransform = DirectX::XMMatrixMultiply(transform.GetTransform(), it->second);
								break;   // cached transform is world-space — stop walking
							}

							Entity parentEntity = FindEntityByUUID(parentUUID);
							if (!parentEntity) 
								break;

							auto& parentTransform = parentEntity.GetComponent<TransformComponent>();
							worldTransform = DirectX::XMMatrixMultiply(worldTransform, parentTransform.GetTransform());
							current = parentEntity;
						}
					}

					bool entityIsHovered = (mHoveredEntity == entity);
					bool entityIsSelected = mRegistry.has<SelectedComponent>(entity);

					auto& lodGroup = mesh.MeshObject->mLODGroups[mesh.MeshObject->mActiveLODGroup];
					auto& submeshes = lodGroup->Submeshes;

					for (uint32_t submeshIndex = 0; submeshIndex < (uint32_t)submeshes.size(); ++submeshIndex)
					{
						Submesh& submesh = submeshes[submeshIndex];

						DirectX::XMMATRIX finalTransform = worldTransform; // fallback

						if (submesh.PartIndex < mesh.MeshObject->mPartsUpdated.size())
						{
							const MeshPart& part = mesh.MeshObject->mPartsUpdated[submesh.PartIndex];

							if (part.EntityID != 0)
							{
								Entity partEntity = FindEntityByUUID(part.EntityID);
								if (partEntity)
								{
									auto& partTransform = partEntity.GetComponent<TransformComponent>();
									DirectX::XMMATRIX animatedTransform = DirectX::XMMatrixMultiply(submesh.Transform, partTransform.GetTransform());
									finalTransform = DirectX::XMMatrixMultiply(animatedTransform, worldTransform);
								}
							}
						}

						switch (mSettings.WireframeRendering)
						{
						case Settings::Wireframe::NO:
							Renderer::SubmitMesh(mesh.MeshObject, finalTransform, (int)entity, submeshIndex, false, 0);
							break;

						case Settings::Wireframe::YES:
							Renderer::SubmitMesh(mesh.MeshObject, finalTransform, (int)entity, submeshIndex, true, 0);
							break;

						case Settings::Wireframe::ONTOP:
							// TODO
							break;
						}

						if (entityIsHovered)
							Renderer::SubmitHoveredMesh(mesh.MeshObject, finalTransform, submeshIndex);

						if (entityIsSelected)
							Renderer::SubmitSelecetedMesh(mesh.MeshObject, finalTransform, false, submeshIndex, true);
					}

					mStats.VerticesCount += static_cast<uint32_t>(mesh.MeshObject->GetVertices().size());
				}

				// Move markers — submit active commands so GuidancePass can draw them.
				{
					auto markerView = mRegistry.view<MoveCommandComponent, MoveableComponent, TransformComponent>();
					for (auto e : markerView)
					{
						auto& cmd = markerView.get<MoveCommandComponent>(e);
						auto& cfg = markerView.get<MoveableComponent>(e);
						auto& tc = markerView.get<TransformComponent>(e);

						Vector3 currentPos(tc.Translation.x, tc.Translation.y, tc.Translation.z);
						double remainingDist = (cmd.TargetWorldPos - currentPos).Length();
						double remainingTime = remainingDist / (double)cmd.Speed;

						float alpha = 1.0f;
						if (remainingTime < cfg.MarkerFadeOutDuration)
							alpha = (float)(remainingTime / cfg.MarkerFadeOutDuration);   // 1.0 → 0.0 as we approach

						Renderer::SubmitMoveMarker(cmd.TargetWorldPos, cmd.TargetSurfaceNormal, cfg.MarkerSize, alpha, cfg.MarkerTextureHandle);
					}
				}

				OutlineSettings outline = ResolveOutlineSettings({});

				Renderer::EndScene(mPlanet, mEnvironment, mSettings.Exposure, mSettings.Bloom, outline, true, mSettings.Shadows.Active, mSettings.SSAO, mSettings.DynamicIBL, *mMainCamera, cameraPosFloat, mSettings.SSAORadius, mSettings.SSAObias, mSettings.GodRays, mSettings.Shadows, mSettings.HoverTintColor, ts, true);
			}

			// Debug Rendering
			RendererDebug::BeginScene(*mMainCamera);
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

					// If the entity is a camera we don't renderer the collider during runtime
					if (!e.HasComponent<CameraComponent>())
					{
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

							DirectX::XMVECTOR localOffset = DirectX::XMVectorSet((float)bcc.Collider->mOffset.x, (float)bcc.Collider->mOffset.y, (float)bcc.Collider->mOffset.z, 0.0f);
							DirectX::XMVECTOR rotatedOffset = DirectX::XMVector3Rotate(localOffset, rot);
							pos = DirectX::XMVectorAdd(pos, rotatedOffset);
						}

						DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScalingFromVector(scale) * DirectX::XMMatrixRotationQuaternion(rot) * DirectX::XMMatrixTranslationFromVector(pos);

						if (renderCollider && mSettings.RenderColliders)
							RendererDebug::SubmitMesh(colliderMesh, transform);
					}
				}
			}
			RendererDebug::EndScene(true, true, true, false);

			// 2D UI Rendering
			Renderer2D::BeginScene(*mMainCamera);
			{
				//Panels
				auto uiPanelEntites = mRegistry.view<TransformComponent, UIPanelComponent>();

				for (auto entity : uiPanelEntites)
				{
					auto [tc, upc] = uiPanelEntites.get<TransformComponent, UIPanelComponent>(entity);
					Entity e{ entity, this };

					if (!upc.Visible)
						continue;

					DirectX::XMFLOAT3 uiPos = tc.Translation;

					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());
						bool parentHasUI = parent.HasComponent<UITextComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UIPanelComponent>();

						if (parentHasUI)
						{
							// UI parent: pure UI-space add
							auto parentUI = parent.GetComponent<TransformComponent>().Translation;
							parentUI.x += (mViewportWidth * 0.5f);
							parentUI.y += (mViewportHeight * 0.5f);

							uiPos.x += parentUI.x;
							uiPos.y += parentUI.y;
							uiPos.z += parentUI.z;
						}

						if (upc.ConnectToParent)
						{
							auto& connector = upc.Connector;

							auto& parentTC = parent.GetComponent<TransformComponent>();

							DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&mMainCamera->GetViewMatrix());
							DirectX::XMMATRIX projectionMatrix = DirectX::XMLoadFloat4x4(&mMainCamera->GetProjection());

							DirectX::XMMATRIX parentM = parentTC.GetTransform();        // includes rotation+translation
							DirectX::XMVECTOR localOffset = DirectX::XMVectorSet(connector.ParentOffset.x, connector.ParentOffset.y, 0.0f, 0.0f);

							DirectX::XMVECTOR worldOffset = DirectX::XMVector3TransformNormal(localOffset, parentM);

							DirectX::XMVECTOR parentCompletePos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&parentTC.Translation), DirectX::XMLoadFloat3(&mMainCamera->GetWorldTranslation()));
							DirectX::XMVECTOR parentWorldPos = DirectX::XMVectorAdd(parentCompletePos, worldOffset);

							DirectX::XMFLOAT3 parentAnchorPos;
							DirectX::XMFLOAT3 panelPosAnchor;

							DirectX::XMVECTOR projected = DirectX::XMVector3Project(parentWorldPos, 0.0f, 0.0f, (float)mViewportWidth, (float)mViewportHeight, 0.0f, 1.0f, projectionMatrix, viewMatrix, DirectX::XMMatrixIdentity());

							panelPosAnchor.x = uiPos.x + (mViewportWidth * 0.5f) + connector.ChildOffset.x;
							panelPosAnchor.y = uiPos.y + (mViewportHeight * 0.5f) + connector.ChildOffset.y;
							panelPosAnchor.z = 1.0f;

							DirectX::XMStoreFloat3(&parentAnchorPos, projected);

							Renderer2D::SubmitConnector(parentAnchorPos, panelPosAnchor, connector.Thickness, 1.0f, connector.Color, -1);
						}
					}

					uiPos.x += (mViewportWidth * 0.5f);
					uiPos.y += (mViewportHeight * 0.5f);

					Renderer2D::SubmitPanel(uiPos, { tc.Scale.x, tc.Scale.y, upc.CornerRadius, 0.0f }, upc.Color, (int)entity, !upc.UseColor, false, upc.TextureIndex);
				}

				//Buttons
				auto uiButtonEntites = mRegistry.view<TransformComponent, UIButtonComponent>();
				for (auto entity : uiButtonEntites)
				{
					auto [tc, ubc] = uiButtonEntites.get<TransformComponent, UIButtonComponent>(entity);

					Entity e{ entity, this };

					if (!ubc.Visible)
						continue;

					bool renderButton = true;

					DirectX::XMFLOAT3 uiPos = tc.Translation;

					Entity current = e;
					while (current.HasParent())
					{
						Entity parent = FindEntityByUUID(current.GetParentUUID());

						// Check if the parent has a UI element component.
						bool parentHasUI = parent.HasComponent<UITextComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UIPanelComponent>();

						if (parentHasUI)
						{
							DirectX::XMFLOAT3 parentUI = parent.GetComponent<TransformComponent>().Translation;
							parentUI.x += (mViewportWidth * 0.5f);
							parentUI.y += (mViewportHeight * 0.5f);

							uiPos.x += parentUI.x;
							uiPos.y += parentUI.y;
							uiPos.z += parentUI.z;

							// If the parent has a UIPanelComponent, check its visibility.
							if (parent.HasComponent<UIPanelComponent>())
							{
								UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();
								if (!parentPanel.Visible)
								{
									renderButton = false;
									break;
								}
							}
						}

						current = parent;
					}

					if (!renderButton)
						continue;

					uiPos.x += (mViewportWidth * 0.5f);
					uiPos.y += (mViewportHeight * 0.5f);

					if (renderButton)
						Renderer2D::SubmitButton(uiPos, { tc.Scale.x, tc.Scale.y, ubc.CornerRadius, 1.0f }, ubc.Color, ubc.ClickColor, (int)entity, !ubc.UseColor, ubc.IsClicked, ubc.TextureIndex, ubc.ClickTextureIndex);
				}

				//Texts
				auto uiTextEntites = mRegistry.view<TransformComponent, UITextComponent>();
				for (auto entity : uiTextEntites)
				{
					auto [tc, uitc] = uiTextEntites.get<TransformComponent, UITextComponent>(entity);

					Entity e{ entity, this };

					bool renderText = true;

					DirectX::XMFLOAT3 uiPos = tc.Translation;

					Entity current = e;
					while (current.HasParent())
					{
						Entity parent = FindEntityByUUID(current.GetParentUUID());

						// Check if the parent has a UI element component.
						bool parentHasUI = parent.HasComponent<UITextComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UIPanelComponent>();

						if (parentHasUI)
						{
							// Add the parent's translation.
							DirectX::XMFLOAT3 parentUI = parent.GetComponent<TransformComponent>().Translation;
							parentUI.x += (mViewportWidth * 0.5f);
							parentUI.y += (mViewportHeight * 0.5f);

							uiPos.x += parentUI.x;
							uiPos.y += parentUI.y;
							uiPos.z += parentUI.z;

							// If the parent has a UIPanelComponent, check its visibility.
							if (parent.HasComponent<UIPanelComponent>())
							{
								UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();
								if (!parentPanel.Visible)
								{
									renderText = false;
									break;
								}
							}
						}

						// Move up one level.
						current = parent;
					}

					if (!renderText)
						continue;

					uiPos.x += (mViewportWidth * 0.5f);
					uiPos.y += (mViewportHeight * 0.5f);
					Renderer2D::SubmitText(uiPos, { tc.Scale.x, tc.Scale.y, 1.0f, 1.0f }, uitc.Color, uitc.Text, uitc.TextureIndex, (int)entity, true);
				}
			}
			Renderer2D::EndScene();
		}
		else 
			TOAST_CORE_ERROR("No main camera! Unable to render scene!");

		// Mouse Picking
		{
			UpdateHoveredEntity();
			UpdatePickedWorldPosition();
		}
	}

	void Scene::OnUpdateEditor(Timestep ts, const Ref<EditorCamera> editorCamera)
	{
		mActiveCamera = editorCamera;

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

				if(mainCameraComponent->IsDirty)
					mPlanet->GetIcosphereMesh()->SetDistanceLUTDirty();
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
			DirectX::XMFLOAT4 direction = { 0.0f, 0.0f, 0.0f, 0.0f };
			DirectX::XMVECTOR sunLightDirWS = XMVectorZero();

			mLightEnvironment = LightEnvironment();
			auto lights = mRegistry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
			uint32_t directionalLightIndex = 0;

			const float camNear = editorCamera->GetNearClip();
			const float camFar = editorCamera->GetFarClip();

			const float shadowFar = std::min(camFar, mSettings.Shadows.ShadowDistance);

			const float fovY = Math::DegreesToRadians(editorCamera->GetVerticalFOV());
			const float aspect = editorCamera->GetAspectRatio();

			float cascadeEnds[MaxCascades] = {};

			const bool shadowsEnabled = mSettings.Shadows.Active;

			if (shadowsEnabled)
			{
				mSettings.Shadows.CascadeCount = std::clamp(mSettings.Shadows.CascadeCount, 1u, MaxCascades);
				mSettings.Shadows.Lambda = std::clamp(mSettings.Shadows.Lambda, 0.0f, 1.0f);

				const float farForShadows = std::max(shadowFar, camNear + 1.0f);

				Renderer::ComputeCascadeEnds(camNear, farForShadows, mSettings.Shadows.CascadeCount, mSettings.Shadows.Lambda, cascadeEnds);

				mSettings.Shadows.IsDirty = false;
			}

			for (auto entity : lights)
			{
				auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);

				DirectX::XMMATRIX transform = transformComponent.GetTransform();

				// Extract forward (Z axis) -> lightDir (same as your code)
				DirectX::XMVECTOR lightDir = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(transform.r[2]));
				sunLightDirWS = lightDir;

				DirectX::XMStoreFloat4(&direction, lightDir);
				direction.w = 0.0f;

				DirectX::XMFLOAT4 radiance(lightComponent.Radiance.x, lightComponent.Radiance.y, lightComponent.Radiance.z, 0.0f);

				// Fill light entry
				auto& out = mLightEnvironment.DirectionalLights[directionalLightIndex++];

				out.Direction = direction;
				out.Radiance = radiance;
				out.Multiplier = lightComponent.Intensity;

				out.CascadeCount = 0;
				out.ShadowDistance = 0.0f;
				std::fill(std::begin(out.CascadeEnds), std::end(out.CascadeEnds), 0.0f);

				if (shadowsEnabled)
				{
					// Store split info on the sun light
					out.CascadeCount = mSettings.Shadows.CascadeCount;
					out.ShadowDistance = shadowFar;
					out.ConstantBias = mSettings.Shadows.ConstantBias;
					out.SlopeBias = mSettings.Shadows.SlopeScaledBias;
					memcpy(out.CascadeEnds, cascadeEnds, sizeof(float) * MaxCascades);

					Renderer::ComputeCSMLightViewProj(editorCamera->GetTranslation(), editorCamera.get(), { editorCamera->GetOrientation() }, fovY, aspect, sunLightDirWS, camNear, out.CascadeEnds, out.CascadeCount, out.ShadowDistance, out.LightViewProj);
				}
				else
				{
					// Fill identity to keep shaders safe
					for (uint32_t i = 0; i < MaxCascades; ++i)
						out.LightViewProj[i] = DirectX::XMMatrixIdentity();
				}
			}

			mEnvironment.SunUV = ComputeSunUVFromDirection(DirectX::XMFLOAT3(direction.x, direction.y, direction.z), DirectX::XMLoadFloat4x4(&editorCamera->GetViewMatrix()), DirectX::XMLoadFloat4x4(&editorCamera->GetProjection()));
		}

		// Process Particles
		{
			auto particleSystem = Renderer::GetParticleSystem();

			std::vector<EmitterParamsGPU> emitterParams;
			std::vector<uint32_t> emitCounts;

			auto view = mRegistry.view<ParticlesComponent>();
			for (auto entity : view)
			{
				if (emitterParams.size() >= MAX_EMITTERS)
				{
					TOAST_CORE_WARN("More than %d particle emitters - ignoring the rest.", MAX_EMITTERS);
					break;
				}

				Entity e = { entity, this };
				ParticlesComponent& pc = e.GetComponent<ParticlesComponent>();
				TransformComponent& tc = e.GetComponent<TransformComponent>();

				// Reset previous spawn position so re-enabling it doesn't smear particles where they shouldn't be
				if (!pc.Emitting)
					pc.HasPrevSpawnPosition = false;

				DirectX::XMFLOAT3 spawnPosition = e.GetComponent<TransformComponent>().Translation;
				DirectX::XMMATRIX rotationMatrix = tc.GetRotation();

				// Inherited world-space velocity from a parent rigid body, if any.
				DirectX::XMFLOAT3 parentVelocity = { 0.0f, 0.0f, 0.0f };

				if (e.HasParent())
				{
					Entity parent = FindEntityByUUID(e.GetParentUUID());
					TransformComponent& parentTC = parent.GetComponent<TransformComponent>();

					DirectX::XMMATRIX parentTransform = parentTC.GetTransformWithoutScale();
					DirectX::XMVECTOR localPos = DirectX::XMLoadFloat3(&spawnPosition);
					DirectX::XMVECTOR worldPos = DirectX::XMVector3Transform(localPos, parentTransform);
					DirectX::XMStoreFloat3(&spawnPosition, worldPos);

					// First frame there is no previous position so we set prev spawn position to spawn position
					if (!pc.HasPrevSpawnPosition)
					{
						pc.PrevSpawnPosition = spawnPosition;
						pc.HasPrevSpawnPosition = true;
					}

					rotationMatrix = DirectX::XMMatrixMultiply(rotationMatrix, parentTC.GetRotation());

					// Velocity inheritance: only if the parent has a rigid body.
					// Restores the behavior dropped during the Step 2 test - this
					// is what stops a fast ship overtaking its own exhaust.
					if (parent.HasComponent<RigidBodyComponent>())
					{
						auto& parentRB = parent.GetComponent<RigidBodyComponent>();
						parentVelocity = {
							(float)parentRB.LinearVelocity.x,
							(float)parentRB.LinearVelocity.y,
							(float)parentRB.LinearVelocity.z
						};
					}
				}

				DirectX::XMFLOAT3 finalVelocity;
				{
					DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pc.Velocity);
					v = DirectX::XMVector3Transform(v, rotationMatrix);
					DirectX::XMStoreFloat3(&finalVelocity, v);
				}

				finalVelocity.x += parentVelocity.x;
				finalVelocity.y += parentVelocity.y;
				finalVelocity.z += parentVelocity.z;

				Renderer::SetParticleMaskTexture(pc.MaskTextureHandle);

				EmitterParamsGPU p = {};
				p.SpawnPosition = spawnPosition;
				p.PrevSpawnPosition = pc.PrevSpawnPosition;
				p.SpawnSize = pc.SpawnBoxSize;
				p.Velocity = finalVelocity;
				p.ConeAngleDegrees = pc.ConeAngleDegrees;
				p.BiasExponent = pc.BiasExponent;
				p.StartColor = pc.StartColor;
				p.EndColor = pc.EndColor;
				p.ColorBlendFactor = pc.ColorBlendFactor;
				p.MaxLifeTime = pc.MaxLifeTime;
				p.Size = pc.Size;
				p.GrowRate = pc.GrowRate;
				p.BurstInitial = pc.BurstInitial;
				p.BurstDecay = pc.BurstDecay;
				p.EmitFunction = static_cast<uint32_t>(pc.SpawnFunction);
				p.SpeedJitter = pc.SpeedJitter;
				p.LifeTimeJitter = pc.LifetimeJitter;
				p.SizeJitter = pc.SizeJitter;
				p.StartIntensity = pc.StartIntensity;
				p.EndIntensity = pc.EndIntensity;
				p.IntensityFalloff = pc.IntensityFalloff;
				p.SoftFadeDistance = pc.SoftFadeDistance;

				emitterParams.push_back(p);
				emitCounts.push_back(ParticleSystem::ComputeEmitCount(pc, ts));

				// Setting up for next frame
				pc.PrevSpawnPosition = spawnPosition;
			}

			particleSystem->OnUpdate(ts, emitterParams, emitCounts);
			//particleSystem->DebugLogCounters(60);
		} // End particle system

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
			if (mainCamera)
			{
				DirectX::XMVECTOR cameraPos, cameraRot, cameraScale;

				DirectX::XMMatrixDecompose(&cameraScale, &cameraRot, &cameraPos, mainCameraTransform->GetTransform());

				InvalidateFrustum();

				mPlanet->OnUpdate(&mainCameraComponent->Camera, { cameraRot }, { cameraPos }, editorCamera->GetTranslation(), mainCameraComponent->Camera.GetWorldTranslation(), mainCameraTransform->GetTransform(), mPhysicsEngine.get(), mFrustum.get(), editorCamera->GetViewMatrix(), mSettings.FrustumCullingMargin);
			}
		}

		DirectX::XMFLOAT4 cameraPosFloat = DirectX::XMFLOAT4(editorCamera->GetTranslation().x, editorCamera->GetTranslation().y, editorCamera->GetTranslation().z, 1.0f);

		// 3D Rendering
		Renderer::BeginScene(this, *editorCamera, cameraPosFloat, mEnvironment, static_cast<int>(mSettings.WireframeRendering));
		{
			// Planet
			Renderer::SubmitPlanet(mPlanet, static_cast<int>(mSettings.WireframeRendering));

			if(mPlanet->GetMeshMode() == PlanetMeshMode::GeometryClipmapping)
			{
				auto& planetMesh = mPlanet->GetGeoClipmapMesh();

				uint64_t v = 0;

				auto& levels = planetMesh->GetLevels();
				auto  info = planetMesh->GetLODDrawInfo();
				uint32_t L0 = info.first;
				uint32_t Ln = L0 + info.count;

				if (levels.size() > 0)
				{
					for (uint32_t L = L0; L < Ln; ++L)
					{
						const auto& level = levels[L];
						if (!level.Dirty && !level.InFrustum)
							continue;

						v += planetMesh->GetLODGridIndexCount(); // drawMode=1 draw

						if (L == L0)  v += planetMesh->GetGridIndexCount();       // center
						else          v += planetMesh->GetRingGridIndexCount();   // ring
					}
				}

				mStats.VerticesCount += (uint32_t)std::min<uint64_t>(v, UINT32_MAX);
			}
			else if (mPlanet->GetMeshMode() == PlanetMeshMode::Icosphere)
			{
				auto& planetMesh = mPlanet->GetIcosphereMesh();

				uint32_t N = 1u << planetMesh->GetPatchLevels(); // N = 2^L
				uint32_t vertsPerPatch = (N + 1u) * (N + 2u) / 2u;

				mStats.VerticesCount += vertsPerPatch * planetMesh->GetPatchCount();
			}

			// Meshes!
			auto viewMeshes = mRegistry.view<TransformComponent, MeshComponent>();
			for (auto entity : viewMeshes)
			{
				auto [transform, mesh] = viewMeshes.get<TransformComponent, MeshComponent>(entity);

				bool validMesh = mesh.MeshObject->GetFilePath() != "";

				if (!validMesh)
					continue;

				// Walk up the relationship tree to get parent transform included.
				DirectX::XMMATRIX worldTransform = transform.GetTransform();
				{
					Entity current{ entity, this };
					while (current.HasComponent<RelationshipComponent>())
					{
						UUID parentUUID = current.GetComponent<RelationshipComponent>().ParentHandle;
						if (parentUUID == 0) break;
						Entity parentEntity = FindEntityByUUID(parentUUID);
						if (!parentEntity) break;
						auto& parentTransform = parentEntity.GetComponent<TransformComponent>();
						worldTransform = DirectX::XMMatrixMultiply(worldTransform, parentTransform.GetTransform());
						current = parentEntity;
					}
				}

				auto& lodGroup = mesh.MeshObject->mLODGroups[mesh.MeshObject->mActiveLODGroup];
				auto& submeshes = lodGroup->Submeshes;

				for (uint32_t submeshIndex = 0; submeshIndex < (uint32_t)submeshes.size(); ++submeshIndex)
				{
					const Submesh& submesh = submeshes[submeshIndex];

					DirectX::XMMATRIX finalTransform = worldTransform; // fallback

					if (submesh.PartIndex < mesh.MeshObject->mPartsUpdated.size())
					{
						const MeshPart& part = mesh.MeshObject->mPartsUpdated[submesh.PartIndex];

						if (part.EntityID != 0)
						{
							Entity partEntity = FindEntityByUUID(part.EntityID);
							if (partEntity)
							{
								auto& partTransform = partEntity.GetComponent<TransformComponent>();
								finalTransform = DirectX::XMMatrixMultiply(partTransform.GetTransform(), worldTransform);
							}
						}
					}

					switch (mSettings.WireframeRendering)
					{
					case Settings::Wireframe::NO:
						Renderer::SubmitMesh(mesh.MeshObject, finalTransform, (int)entity, submeshIndex, false, 0);
						break;

					case Settings::Wireframe::YES:
						Renderer::SubmitMesh(mesh.MeshObject, finalTransform, (int)entity, submeshIndex, true, 0);
						break;

					case Settings::Wireframe::ONTOP:
						// TODO
						break;
					}

					if (mSelectedEntity == entity)
						Renderer::SubmitSelecetedMesh(mesh.MeshObject, finalTransform, false, submeshIndex, false);
				}

				mStats.VerticesCount += static_cast<uint32_t>(mesh.MeshObject->GetVertices().size());
			}

			OutlineSettings outline = ResolveOutlineSettings({});

			Renderer::EndScene(mPlanet, mEnvironment, mSettings.Exposure, mSettings.Bloom, outline, true, mSettings.Shadows.Active, mSettings.SSAO, mSettings.DynamicIBL, *editorCamera, cameraPosFloat, mSettings.SSAORadius, mSettings.SSAObias, mSettings.GodRays, mSettings.Shadows, mSettings.HoverTintColor, ts, false);
		}

		// Debug Rendering
		RendererDebug::BeginScene(*editorCamera);
		{
			// Frustum
			if (mSettings.CameraFrustum && mFrustum)
				RendererDebug::SubmitCameraFrustum(mFrustum);

			// SSAO Debugging
			if(mSettings.SSAODebugging)
			{
				const uint32_t NOISE_DIM = 16;
				int centerX = static_cast<int>(mViewportWidth * 0.5f);
				int centerY = static_cast<int>(mViewportHeight * 0.5f);

				float u = (centerX / (float)mViewportWidth) * NOISE_DIM;
				float v = (centerY / (float)mViewportHeight) * NOISE_DIM;

				int noiseX = static_cast<int>(floorf(u)) % NOISE_DIM;
				int noiseY = static_cast<int>(floorf(v)) % NOISE_DIM;

				DirectX::XMFLOAT2 noiseScale = DirectX::XMFLOAT2(mViewportWidth / 16.0, mViewportHeight / 16.0);

				DirectX::XMFLOAT4 posTextureValue = Renderer::GetGPassPositionRT()->ReadPixel<DirectX::XMFLOAT4>(centerX, centerY);
				DirectX::XMFLOAT3 debugPosViewSpace = DirectX::XMFLOAT3(posTextureValue.x, posTextureValue.y, posTextureValue.z);
				DirectX::XMVECTOR debugPosViewSpaceVec = DirectX::XMLoadFloat3(&debugPosViewSpace);

				DirectX::XMFLOAT4 normalTextureValue = Renderer::GetGPassNormalRT()->ReadPixel<DirectX::XMFLOAT4>(centerX, centerY);
				DirectX::XMFLOAT3 debugNormalViewSpace = DirectX::XMFLOAT3(normalTextureValue.x, normalTextureValue.y, normalTextureValue.z);
				DirectX::XMVECTOR debugNormalViewSpaceVec = DirectX::XMLoadFloat3(&debugNormalViewSpace);
				debugNormalViewSpaceVec = DirectX::XMVector3Normalize(
					DirectX::XMVectorSubtract(DirectX::XMVectorScale(debugNormalViewSpaceVec, 2.0f), DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f)));

				DirectX::XMFLOAT3 randomNoise = Renderer::SampleSSAONoiseTexture(noiseX, noiseY);
				DirectX::XMVECTOR randomNoiseVec = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&randomNoise));

				float dotNV = DirectX::XMVectorGetX(DirectX::XMVector3Dot(randomNoiseVec, debugNormalViewSpaceVec));
				DirectX::XMVECTOR tangent = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(randomNoiseVec, DirectX::XMVectorScale(debugNormalViewSpaceVec, dotNV)));

				DirectX::XMVECTOR bitangent = DirectX::XMVector3Cross(debugNormalViewSpaceVec, tangent);

				DirectX::XMMATRIX TBN(tangent, bitangent, debugNormalViewSpaceVec, DirectX::XMVectorSet(0, 0, 0, 1));

				DirectX::XMVECTOR debugPointWorld = DirectX::XMVector3TransformCoord(debugPosViewSpaceVec, DirectX::XMLoadFloat4x4(&editorCamera->GetInvViewMatrix()));

				for (const auto& sample : Renderer::GetSSAOKernel())
				{
					DirectX::XMVECTOR sampleDirView = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&sample), TBN);
					DirectX::XMVECTOR sampleEndView = DirectX::XMVectorAdd(debugPosViewSpaceVec, DirectX::XMVectorScale(sampleDirView, mSettings.SSAORadius));
					DirectX::XMVECTOR sampleEndWorld = DirectX::XMVector3TransformCoord(sampleEndView, DirectX::XMLoadFloat4x4(&editorCamera->GetInvViewMatrix()));

					DirectX::XMFLOAT3 sampleEndWorldFloat, debugPointWorldFloat;
					DirectX::XMStoreFloat3(&debugPointWorldFloat, debugPointWorld);
					DirectX::XMStoreFloat3(&sampleEndWorldFloat, sampleEndWorld);

					RendererDebug::SubmitLine(debugPointWorldFloat, sampleEndWorldFloat, DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f));
				}
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

					DirectX::XMVECTOR localOffset = DirectX::XMVectorSet((float)bcc.Collider->mOffset.x, (float)bcc.Collider->mOffset.y, (float)bcc.Collider->mOffset.z, 0.0f);
					DirectX::XMVECTOR rotatedOffset = DirectX::XMVector3Rotate(localOffset, rot);
					pos = DirectX::XMVectorAdd(pos, rotatedOffset);
				}

				DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScalingFromVector(scale) * DirectX::XMMatrixRotationQuaternion(rot) * DirectX::XMMatrixTranslationFromVector(pos);

				if (renderCollider && mSettings.RenderColliders)
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

			// Center of Mass guide
			auto viewEntitiesCoM = mRegistry.view<TransformComponent, RigidBodyComponent>();
			for (auto entity : viewEntitiesCoM)
			{
				Entity e{ entity, this };

				auto rbc = e.GetComponent<RigidBodyComponent>();

				DirectX::XMMATRIX comOffset = DirectX::XMMatrixTranslation(rbc.CenterOfMass.x, rbc.CenterOfMass.y, rbc.CenterOfMass.z);
				DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(comOffset, e.GetComponent<TransformComponent>().GetTransform());

				if (e.HasParent())
				{
					Entity parent = FindEntityByUUID(e.GetParentUUID());

					DirectX::XMMATRIX& parentTransform = parent.GetComponent<TransformComponent>().GetTransform();
					transform = DirectX::XMMatrixMultiply(transform, parentTransform);
				}

				RendererDebug::SubmitMesh(rbc.GuideMesh, transform, false);
			}
		}

		RendererDebug::EndScene(true, false, mSettings.RenderUI, mSettings.Grid);

		// 2D UI Rendering
		if (mSettings.RenderUI) 
		{
			Renderer2D::BeginScene(*editorCamera);
			{
				//Panels
				auto uiPanelEntites = mRegistry.view<TransformComponent, UIPanelComponent>();

				for (auto entity : uiPanelEntites)
				{
					auto [tc, upc] = uiPanelEntites.get<TransformComponent, UIPanelComponent>(entity);
					Entity e{ entity, this };

					if (!upc.Visible)
						continue;

					DirectX::XMFLOAT3 uiPos = tc.Translation;

					if (e.HasParent())
					{
						Entity parent = FindEntityByUUID(e.GetParentUUID());
						bool parentHasUI = parent.HasComponent<UITextComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UIPanelComponent>();

						if (parentHasUI)
						{
							// UI parent: pure UI-space add
							auto parentUI = parent.GetComponent<TransformComponent>().Translation;
							parentUI.x += (mViewportWidth * 0.5f);
							parentUI.y += (mViewportHeight * 0.5f);

							uiPos.x += parentUI.x;
							uiPos.y += parentUI.y;
							uiPos.z += parentUI.z;
						}

						if (upc.ConnectToParent)
						{
							auto& connector = upc.Connector;

							auto& parentTC = parent.GetComponent<TransformComponent>();

							DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&editorCamera->GetViewMatrix());
							DirectX::XMMATRIX projectionMatrix = DirectX::XMLoadFloat4x4(&editorCamera->GetProjection());

							DirectX::XMMATRIX parentM = parentTC.GetTransform();        // includes rotation+translation
							DirectX::XMVECTOR localOffset = DirectX::XMVectorSet(connector.ParentOffset.x, connector.ParentOffset.y, 0.0f, 0.0f); 

							DirectX::XMVECTOR worldOffset = DirectX::XMVector3TransformNormal(localOffset, parentM);
							DirectX::XMVECTOR parentWorldPos = DirectX::XMVectorAdd(XMLoadFloat3(&parentTC.Translation), worldOffset);

							DirectX::XMFLOAT3 parentAnchorPos;
							DirectX::XMFLOAT3 panelPosAnchor;

							DirectX::XMVECTOR projected = DirectX::XMVector3Project(parentWorldPos,	0.0f, 0.0f,	(float)mViewportWidth, (float)mViewportHeight, 0.0f, 1.0f, projectionMatrix, viewMatrix, DirectX::XMMatrixIdentity());

							panelPosAnchor.x = uiPos.x + (mViewportWidth * 0.5f) + connector.ChildOffset.x;
							panelPosAnchor.y = uiPos.y + (mViewportHeight * 0.5f) + connector.ChildOffset.y;
							panelPosAnchor.z = 1.0f;

							DirectX::XMStoreFloat3(&parentAnchorPos, projected);

							Renderer2D::SubmitConnector(parentAnchorPos, panelPosAnchor, connector.Thickness, 1.0f, connector.Color, -1);
						}
					}

					uiPos.x += (mViewportWidth * 0.5f);
					uiPos.y += (mViewportHeight * 0.5f);

					Renderer2D::SubmitPanel(uiPos, { tc.Scale.x, tc.Scale.y, upc.CornerRadius, 0.0f }, upc.Color, (int)entity, !upc.UseColor, false, upc.TextureIndex);
				}

				//Buttons
				auto uiButtonEntites = mRegistry.view<TransformComponent, UIButtonComponent>();
				for (auto entity : uiButtonEntites)
				{
					auto [tc, ubc] = uiButtonEntites.get<TransformComponent, UIButtonComponent>(entity);
					Entity e{ entity, this };

					if (!ubc.Visible)
						continue;

					bool renderButton = true;

					DirectX::XMFLOAT3 uiPos = tc.Translation;

					Entity current = e;
					while (current.HasParent())
					{
						Entity parent = FindEntityByUUID(current.GetParentUUID());

						// Check if the parent has a UI element component.
						bool parentHasUI = parent.HasComponent<UITextComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UIPanelComponent>();

						if (parentHasUI)
						{
							DirectX::XMFLOAT3 parentUI = parent.GetComponent<TransformComponent>().Translation;
							parentUI.x += (mViewportWidth * 0.5f);
							parentUI.y += (mViewportHeight * 0.5f);

							uiPos.x += parentUI.x;
							uiPos.y += parentUI.y;
							uiPos.z += parentUI.z;

							// If the parent has a UIPanelComponent, check its visibility.
							if (parent.HasComponent<UIPanelComponent>())
							{
								UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();
								if (!parentPanel.Visible)
								{
									renderButton = false;
									break;
								}
							}
						}

						current = parent;
					}

					if (!renderButton)
						continue;

					uiPos.x += (mViewportWidth * 0.5f);
					uiPos.y += (mViewportHeight * 0.5f);

					if(renderButton)
						Renderer2D::SubmitButton(uiPos, { tc.Scale.x, tc.Scale.y, ubc.CornerRadius, 1.0f }, ubc.Color, ubc.ClickColor, (int)entity, !ubc.UseColor, ubc.IsClicked, ubc.TextureIndex, ubc.ClickTextureIndex);
				}

				//Texts
				auto uiTextEntites = mRegistry.view<TransformComponent, UITextComponent>();
				for (auto entity : uiTextEntites)
				{
					auto [tc, uitc] = uiTextEntites.get<TransformComponent, UITextComponent>(entity);

					Entity e{ entity, this };

					bool renderText = true;

					DirectX::XMFLOAT3 uiPos = tc.Translation;

					Entity current = e;
					while (current.HasParent())
					{
						Entity parent = FindEntityByUUID(current.GetParentUUID());

						// Check if the parent has a UI element component.
						bool parentHasUI = parent.HasComponent<UITextComponent>() || parent.HasComponent<UIButtonComponent>() || parent.HasComponent<UIPanelComponent>();

						if (parentHasUI)
						{
							// Add the parent's translation.
							DirectX::XMFLOAT3 parentUI = parent.GetComponent<TransformComponent>().Translation;
							parentUI.x += (mViewportWidth * 0.5f);
							parentUI.y += (mViewportHeight * 0.5f);

							uiPos.x += parentUI.x;
							uiPos.y += parentUI.y;
							uiPos.z += parentUI.z;

							// If the parent has a UIPanelComponent, check its visibility.
							if (parent.HasComponent<UIPanelComponent>())
							{
								UIPanelComponent parentPanel = parent.GetComponent<UIPanelComponent>();
								if (!parentPanel.Visible)
								{
									renderText = false;
									break;
								}
							}
						}
						// Move up one level.
						current = parent;
					}

					if (!renderText)
						continue;

					uiPos.x += (mViewportWidth * 0.5f);
					uiPos.y += (mViewportHeight * 0.5f);
					Renderer2D::SubmitText(uiPos, { tc.Scale.x, tc.Scale.y, 1.0f, 1.0f }, uitc.Color, uitc.Text, uitc.TextureIndex, (int)entity, true);
				}
			}
			Renderer2D::EndScene();
		}

		// Mouse Picking
		{
			UpdateHoveredEntity();
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
				cameraComponent.Camera.SetViewportSize(width, height);
		}
	}

	void Scene::SetViewportBounds(DirectX::XMFLOAT2 viewportBounds[2])
	{
		for (int i = 0; i < 2; ++i)
			mViewportBounds[i] = viewportBounds[i];
	}

	void Scene::InvalidateFrustum()
	{
		auto view = mRegistry.view<TransformComponent, CameraComponent>();
		for (auto entity : view)
		{
			auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

			if (camera.Primary)
			{
				mFrustum->Invalidate(camera.Camera.GetAspecRatio(), camera.Camera.GetPerspectiveVerticalFOV(), camera.Camera.GetNearClip(), camera.Camera.GetFarClip());
				mFrustum->Update(Matrix(transform.GetTransform()), Matrix(mPlanet->GetTransformNoScale()), mSettings.FrustumCullingMargin);
			}
		}
	}

	float Scene::GetAltitude(Entity& entity, bool ignoreWorldTranslation)
	{
		return mPhysicsEngine->GetAltitude(entity, ignoreWorldTranslation);
	}

	float Scene::GetAltitudeAtWorldPos(const Vector3& worldPos, double& outRadialDist, Vector3& outGroundNormal)
	{
		return mPhysicsEngine->GetAltitudeAtWorldPos(worldPos, outRadialDist, outGroundNormal);
	}

	Ref<Scene> Scene::CreateEmpty()
	{
		return CreateRef<Scene>();
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

	Entity Scene::FindDescendantByName(Entity parent, std::string_view nameStr)
	{
		if (!parent)
			return {};

		std::vector<Entity> stack;
		stack.reserve(64);

		// Seed with root's children (root itself is NOT considered; add if you want)
		if (parent.HasComponent<RelationshipComponent>())
		{
			auto& rc = parent.GetComponent<RelationshipComponent>().Children;
			for (UUID childUUID : rc)
			{
				Entity child = FindEntityByUUID(childUUID);   // <-- must exist in your Scene
				if (child) 
					stack.push_back(child);
			}
		}


		while (!stack.empty())
		{
			Entity e = stack.back();
			stack.pop_back();

			// Match by name
			if (e.HasComponent<TagComponent>() && e.GetComponent<TagComponent>().Tag == nameStr)
				return e;

			// Push children
			if (e.HasComponent<RelationshipComponent>())
			{
				auto& ch = e.GetComponent<RelationshipComponent>().Children;
				for (UUID childUUID : ch)
				{
					Entity child = FindEntityByUUID(childUUID);
					if (child) 
						stack.push_back(child);
				}
			}
		}

		return {};
	}

	Entity Scene::FindParentEntity(UUID childID)
	{
		Entity child = FindEntityByUUID(childID);
		if (!child)
			return Entity{};

		if (!child.HasComponent<RelationshipComponent>())
			return Entity{};

		const auto& rel = child.GetComponent<RelationshipComponent>();

		// Adapt this field name to your actual RelationshipComponent
		const UUID parentID = rel.ParentHandle;

		if ((uint64_t)parentID == 0)
			return Entity{};

		return FindEntityByUUID(parentID);
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

	void Scene::AddMeshPartEntities(std::vector<MeshPart>& parts, Entity& meshParent)
	{
		for (size_t i = 0; i < parts.size(); ++i)
		{
			MeshPart& part = parts[i];

			Entity partEntity = CreateEntity(part.Name, meshParent.GetUUID());
			part.EntityID = partEntity.GetUUID();

			partEntity.AddComponent<MeshPartComponent>();

			auto& tc = partEntity.GetComponent<TransformComponent>();
			tc.Translation = part.InitialTranslation;
			tc.RotationQuaternion = part.InitialRotation; 

			tc.Scale = part.InitialScale;

			partEntity.SetParentUUID(meshParent.GetUUID());
			meshParent.Children().push_back(partEntity.GetUUID());
		}
	}

	DirectX::XMMATRIX Scene::GetWorldTransform(Entity entity)
	{
		DirectX::XMMATRIX world = entity.GetComponent<TransformComponent>().GetTransform();
		Entity current = entity;

		while (current.HasComponent<RelationshipComponent>())
		{
			UUID pUUID = current.GetComponent<RelationshipComponent>().ParentHandle;
			if (pUUID == 0) break;

			Entity parentEntity = FindEntityByUUID(pUUID);
			if (!parentEntity) break;

			// Default: the parent's own rest transform.
			DirectX::XMMATRIX parentTransform = parentEntity.GetComponent<TransformComponent>().GetTransform();

			// But if this parent is an ANIMATED PART of some mesh, the animation lives in
			// the matching submesh's Transform, NOT in the part-entity's TransformComponent.
			// Find that submesh and compose its animated transform: submesh.Transform * partRest.
			DirectX::XMMATRIX animatedPartTransform;
			if (FindAnimatedPartTransform(pUUID, parentTransform, animatedPartTransform))
				parentTransform = animatedPartTransform;

			world = DirectX::XMMatrixMultiply(world, parentTransform);
			current = parentEntity;
		}

		return world;
	}

	// Returns true if pUUID is a part-entity of some mesh; outputs submesh.Transform * partRest.
	bool Scene::FindAnimatedPartTransform(UUID partEntityUUID, const DirectX::XMMATRIX& partRest, DirectX::XMMATRIX& out)
	{
		auto meshView = mRegistry.view<MeshComponent>();
		for (auto e : meshView)
		{
			auto& mesh = meshView.get<MeshComponent>(e);
			if (mesh.MeshObject->GetFilePath() == "") continue;

			auto& submeshes = mesh.MeshObject->mLODGroups[mesh.MeshObject->mActiveLODGroup]->Submeshes;
			for (auto& submesh : submeshes)
			{
				if (submesh.PartIndex >= mesh.MeshObject->mPartsUpdated.size()) continue;
				const MeshPart& part = mesh.MeshObject->mPartsUpdated[submesh.PartIndex];
				if (part.EntityID == partEntityUUID)
				{
					// Match the render loop's composition: submesh.Transform * partRest
					out = DirectX::XMMatrixMultiply(submesh.Transform, partRest);
					return true;
				}
			}
		}
		return false;
	}

	void Scene::UnparentEntity(Entity entity)
	{
		if (!entity.HasComponent<RelationshipComponent>()) return;
		auto& rc = entity.GetComponent<RelationshipComponent>();
		UUID parentUUID = rc.ParentHandle;
		if (parentUUID == 0) return;

		// Bake world transform into local so the entity doesn't jump on detach.
		DirectX::XMMATRIX world = GetWorldTransform(entity);
		DirectX::XMVECTOR scale, rotQuat, trans;
		if (DirectX::XMMatrixDecompose(&scale, &rotQuat, &trans, world))
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			DirectX::XMStoreFloat3(&tc.Translation, trans);
			DirectX::XMStoreFloat4(&tc.RotationQuaternion, DirectX::XMQuaternionNormalize(rotQuat));
			tc.RotationEulerAngles = { 0.0f, 0.0f, 0.0f };
			DirectX::XMStoreFloat3(&tc.Scale, scale);
			tc.IsDirty = true;
		}

		Entity parent = FindEntityByUUID(parentUUID);
		if (parent) parent.RemoveChild(entity);
		rc.ParentHandle = 0;
	}

	template<typename T>
	static void CopyComponentIfExists(entt::entity dst, entt::registry& dstRegistry, entt::entity src, entt::registry& srcRegistry)
	{
		if (srcRegistry.has<T>(src))
		{
			auto& srcComponent = srcRegistry.get<T>(src);
			dstRegistry.emplace_or_replace<T>(dst, srcComponent);
		}
	}

	uint32_t Scene::GetNextPrefabIndex(const std::string& prefabName)
	{
		uint32_t count = 0;
		auto view = mRegistry.view<PrefabComponent>();
		for (auto e : view)
		{
			const auto& pc = view.get<PrefabComponent>(e);
			if (pc.PrefabHandle == prefabName) // adapt to your field name
				count++;
		}
		return count + 1;
	}

	Entity Scene::AddPrefab(std::string& prefabName)
	{
		std::vector<Entity> prefabEntities = PrefabLibrary::GetEntities(prefabName);

		// Mapping from old prefab UUID to new entity.
		std::unordered_map<UUID, Entity> mapping;
		// Mapping from old prefab UUID to its original children list.
		std::unordered_map<UUID, std::vector<UUID>> oldChildrenMapping;

		uint32_t index = GetNextPrefabIndex(prefabName);
		std::string instanceName = prefabName + "_" + std::to_string(index);

		// ----- First Pass: Create new entities and store mapping -----

		// Process the root prefab entity.
		Entity prefabRoot = prefabEntities[0];
		UUID prefabRootID = prefabRoot.GetUUID();
		Entity newRootEntity = CreateEntity(instanceName);
		newRootEntity.AddComponent<PrefabComponent>(prefabName);
		mapping[prefabRootID] = newRootEntity;

		// If the prefab root has a RelationshipComponent, record its children.
		if (prefabRoot.HasComponent<RelationshipComponent>())
			oldChildrenMapping[prefabRootID] = prefabRoot.GetComponent<RelationshipComponent>().Children;

		// Copy the RelationshipComponent (but clear the children list)
		CopyComponentIfExists<RelationshipComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		if (newRootEntity.HasComponent<RelationshipComponent>())
			newRootEntity.GetComponent<RelationshipComponent>().Children.clear();

		// Copy the remaining components from the prefab root.
		CopyComponentIfExists<TransformComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<MeshComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<MeshPartComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<CameraComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<SpriteRendererComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<DirectionalLightComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<ScriptComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<SceneScriptComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<RigidBodyComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<SphereColliderComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<BoxColliderComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<UIPanelComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<UITextComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<UIButtonComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<ParticlesComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);
		CopyComponentIfExists<MoveableComponent>(newRootEntity, mRegistry, prefabRoot, prefabRoot.mScene->mRegistry);

		// Process the rest of the prefab entities.
		for (size_t i = 1; i < prefabEntities.size(); ++i)
		{
			Entity prefabEntity = prefabEntities[i];
			UUID prefabID = prefabEntity.GetUUID();
			std::string tagName = prefabEntity.HasComponent<TagComponent>() ?
				prefabEntity.GetComponent<TagComponent>().Tag : "Prefab Child";
			Entity newEntity = CreateEntity(tagName);
			mapping[prefabID] = newEntity;

			// Record the original children from the prefab's RelationshipComponent.
			if (prefabEntity.HasComponent<RelationshipComponent>())
				oldChildrenMapping[prefabID] = prefabEntity.GetComponent<RelationshipComponent>().Children;

			// Copy the RelationshipComponent and clear its children list.
			CopyComponentIfExists<RelationshipComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			if (newEntity.HasComponent<RelationshipComponent>())
				newEntity.GetComponent<RelationshipComponent>().Children.clear();

			// Copy the remaining components.
			CopyComponentIfExists<TransformComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<MeshComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<MeshPartComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<CameraComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<SpriteRendererComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<DirectionalLightComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<ScriptComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<SceneScriptComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<RigidBodyComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<SphereColliderComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<BoxColliderComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<UIPanelComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<UITextComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<UIButtonComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<ParticlesComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
			CopyComponentIfExists<MoveableComponent>(newEntity, mRegistry, prefabEntity, prefabEntity.mScene->mRegistry);
		}

		// ----- Second Pass: Update parent-child relationships using the mapping -----

		for (auto& [oldID, newEntity] : mapping)
		{
			// If this prefab entity had children...
			if (oldChildrenMapping.find(oldID) != oldChildrenMapping.end())
			{
				std::vector<UUID> newChildren;
				// For each child UUID in the prefab...
				for (UUID oldChildID : oldChildrenMapping[oldID])
				{
					// Check if a new entity was created for that child.
					if (mapping.find(oldChildID) != mapping.end())
					{
						Entity childEntity = mapping[oldChildID];
						newChildren.push_back(childEntity.GetUUID());
						// Update the child's parent pointer.
						childEntity.SetParentUUID(newEntity.GetUUID());
					}
				}
				// Update the current new entity's RelationshipComponent with the remapped children.
				if (newEntity.HasComponent<RelationshipComponent>())
					newEntity.GetComponent<RelationshipComponent>().Children = newChildren;
			}
		}


		// Fixing the script instances to make sure the Script Engine can run the prefab scripts
		for (auto& [oldID, newEntity] : mapping)
		{
			if (newEntity.HasComponent<ScriptComponent>())
				ScriptEngine::OnCreateEntity(newEntity);
		}

		return newRootEntity;
	}

	std::vector<Toast::Entity> Scene::GetEntitiesWithPrefab(std::string prefabName)
	{
		std::vector<Entity> result;
		auto view = mRegistry.view<PrefabComponent>();

		for (auto entityID : view)
		{
			const auto& prefab = view.get<PrefabComponent>(entityID);
			if (prefab.PrefabHandle == prefabName)           
				result.emplace_back(Entity{ entityID, this });
		}

		return result;
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

	void Scene::CopyTo(Scene* target)
	{
		// Settings
		target->mSettings.SSAO = mSettings.SSAO;
		target->mSettings.SSAObias = mSettings.SSAObias;
		target->mSettings.SSAORadius = mSettings.SSAORadius;
		target->mSettings.Bloom = mSettings.Bloom;
		target->mSettings.Exposure = mSettings.Exposure;
		target->mSettings.DirectionalLightningGain = mSettings.DirectionalLightningGain;
		target->mSettings.Outline = mSettings.Outline;
		target->mSettings.HoverTintColor = mSettings.HoverTintColor;

		// Physics Settings
		auto& targetPhysics = target->GetPhysicsEngine();
		targetPhysics->mSettings.SlowDown = mPhysicsEngine->mSettings.SlowDown;
		targetPhysics->mSettings.FPSTarget = mPhysicsEngine->mSettings.FPSTarget;
		targetPhysics->mSettings.StepsPerUpdate = mPhysicsEngine->mSettings.StepsPerUpdate;

		// Environment
		target->mEnvironment = mEnvironment;
		target->mLightEnvironment = mLightEnvironment;

		target->mSkyboxTexture = mSkyboxTexture;
		target->mSkyboxLod = mSkyboxLod;

		// Planet
		target->mPlanet = mPlanet;

		// Collider
		target->mSettings.RenderColliders = mSettings.RenderColliders;
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

		// Camera
		target->mMainCamera = mMainCamera;

		CopyComponent<RelationshipComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<TagComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<PrefabComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<TransformComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<MeshComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<MeshPartComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<CameraComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<DirectionalLightComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<ScriptComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SceneScriptComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<RigidBodyComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<SphereColliderComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<BoxColliderComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<UIPanelComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<UITextComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<UIButtonComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<ParticlesComponent>(target->mRegistry, mRegistry, enttMap);
		CopyComponent<MoveableComponent>(target->mRegistry, mRegistry, enttMap);
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component) 
	{
		static_assert(false); 
	}

	void Scene::UpdateHoveredEntity()
	{
		TOAST_PROFILE_FUNCTION();

		Ref<RenderTarget>& pickingRT = Renderer::GetGPassPickingRT();

		float viewportX = mViewportBounds[0].x;
		float viewportY = mViewportBounds[0].y;
		float viewportWidth = mViewportBounds[1].x - mViewportBounds[0].x;
		float viewportHeight = mViewportBounds[1].y - mViewportBounds[0].y;

		float adjustedX = mMouseX - viewportX;
		float adjustedY = mMouseY - viewportY;

		// Outside viewport — clear hover and skip
		if (adjustedX < 0 || adjustedY < 0 ||
			adjustedX >= viewportWidth || adjustedY >= viewportHeight)
		{
			mHoveredEntity = Entity();
			mPickingReadbackPending = false;
			return;
		}

		// Map the staging texture from LAST frame's copy (no stall — it's long done)
		if (mPickingReadbackPending)
		{
			int pixelData = pickingRT->MapStagingPixel<int>();
			mHoveredEntity = (pixelData == 0)
				? Entity()
				: Entity{ (entt::entity)(pixelData - 1), this };
		}

		// Queue THIS frame's 1×1 copy — returns immediately, executes on GPU later
		auto [rtWidth, rtHeight] = pickingRT->GetSize();
		int textureX = static_cast<int>((adjustedX / viewportWidth) * rtWidth);
		int textureY = static_cast<int>((adjustedY / viewportHeight) * rtHeight);

		pickingRT->CopyPixelToStaging(textureX, textureY);
		mPickingReadbackPending = true;
	}

	void Scene::UpdatePickedWorldPosition()
	{
		Ref<RenderTarget>& posRT = Renderer::GetGPassPositionRT();   // add this accessor like GetGPassPickingRT

		float viewportX = mViewportBounds[0].x;
		float viewportY = mViewportBounds[0].y;
		float viewportWidth = mViewportBounds[1].x - mViewportBounds[0].x;
		float viewportHeight = mViewportBounds[1].y - mViewportBounds[0].y;
		float adjustedX = mMouseX - viewportX;
		float adjustedY = mMouseY - viewportY;

		if (adjustedX < 0 || adjustedY < 0 || adjustedX >= viewportWidth || adjustedY >= viewportHeight)
		{
			mPositionReadbackPending = false;
			mLastPickedValid = false;
			return;
		}

		// Read LAST frame's copy (no stall)
		if (mPositionReadbackPending)
		{
			struct Float4 { float x, y, z, w; };
			Float4 viewPos = posRT->MapStagingPixel<Float4>();

			// Cleared to (0,0,0,1) => sky / no geometry
			if (viewPos.x == 0.0f && viewPos.y == 0.0f && viewPos.z == 0.0f)
			{
				mLastPickedValid = false;
			}
			else
			{
				// View space -> camera-relative world (inverse view, row-vector mul)
				DirectX::XMVECTOR vp = DirectX::XMVectorSet(viewPos.x, viewPos.y, viewPos.z, 1.0f);
				DirectX::XMMATRIX invView = DirectX::XMLoadFloat4x4(&mMainCamera->GetInvViewMatrix());
				DirectX::XMVECTOR wp = DirectX::XMVector4Transform(vp, invView);

				Vector3 camRelative(DirectX::XMVectorGetX(wp), DirectX::XMVectorGetY(wp), DirectX::XMVectorGetZ(wp));
				Vector3 lastPickedWorldPos = camRelative;// +mMainCamera->GetWorldTranslation();
				mLastPickedWorldPos = { (float)lastPickedWorldPos.x, (float)lastPickedWorldPos.y, (float)lastPickedWorldPos.z };   // true-world
				mLastPickedValid = true;
			}
		}

		// Queue THIS frame's copy
		auto [rtWidth, rtHeight] = posRT->GetSize();
		int textureX = static_cast<int>((adjustedX / viewportWidth) * rtWidth);
		int textureY = static_cast<int>((adjustedY / viewportHeight) * rtHeight);
		posRT->CopyPixelToStaging(textureX, textureY);
		mPositionReadbackPending = true;
	}

	bool Scene::GetWorldPositionUnderCursor(Vector3& outWorldPos)
	{
		if (!mLastPickedValid) return false;
		outWorldPos = mLastPickedWorldPos;
		return true;
	}

	Scene::OutlineSettings Scene::ResolveOutlineSettings(Entity selected)
	{
		// In the future this will resolve priority between entity, scene and project setting
		return GetOutlineSettings();   // A5 pass through
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
	void Scene::OnComponentAdded<PrefabComponent>(Entity entity, PrefabComponent& component)
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
	void Scene::OnComponentAdded<MeshPartComponent>(Entity entity, MeshPartComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		auto tc = entity.GetComponent<TransformComponent>();
		Vector3 cameraTranslation = { tc.Translation };

		if(mViewportWidth > 0 && mViewportHeight > 0)
			component.Camera.SetViewportSize(mViewportWidth, mViewportHeight);

		mFrustum = CreateRef<Frustum>();
		mFrustum->Invalidate(component.Camera.GetAspecRatio(), component.Camera.GetPerspectiveVerticalFOV(), component.Camera.GetNearClip(), component.Camera.GetFarClip());

		if (component.Primary)
			mMainCamera = &component.Camera;
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
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SceneScriptComponent>(Entity entity, SceneScriptComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<RigidBodyComponent>(Entity entity, RigidBodyComponent& component)
	{
		component.GuideMesh = MeshFactory::CreateCube(1.0f, { 1.0, 0.0, 1.0 });
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
	void Scene::OnComponentAdded<ParticlesComponent>(Entity entity, ParticlesComponent& component)
	{
		component.GuideMesh = MeshFactory::CreateCube(1.0f, { 1.0, 0.0, 0.0 });
	}

	template<>
	void Scene::OnComponentAdded<SelectedComponent>(Entity entity, SelectedComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<MoveableComponent>(Entity entity, MoveableComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<MoveCommandComponent>(Entity entity, MoveCommandComponent& component)
	{
	}
}