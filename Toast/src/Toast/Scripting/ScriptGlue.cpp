#include "tpch.h"
#include "ScriptGlue.h"
#include "ScriptEngine.h"

#include "Toast/Core/Application.h"

#include "Toast/Renderer/PlanetSystem.h"

#include "Toast/Physics/PhysicsEngine.h"

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"
#include "Toast/Scene/ISceneProvider.h" 
#include "Toast/Scene/SelectionSystem.h"

#include "mono/metadata/appdomain.h"
#include "mono/metadata/object.h"
#include "mono/metadata/reflection.h"

#include <DirectXMath.h>

namespace Toast {

	namespace Utils
	{
		std::string ConvertMonoStringToCppString(MonoString* msg)
		{
			char* ptr = mono_string_to_utf8(msg);
			std::string s(ptr);
			mono_free(ptr);
			return s;
		}

		char* ConvertMonoObjectToCppChar(MonoObject* obj)
		{
			if (obj == NULL)
			{
				char* a = "NULL";
				return a;
			}
			else
			{
				MonoString* a = mono_object_to_string(obj, NULL);
				std::string b = ConvertMonoStringToCppString(a);
				char* s = _strdup(b.c_str());
				return s;
			}
		}

		MonoString* ConvertCppStringToMonoString(MonoDomain* domain, const std::string& str)
		{
			return mono_string_new(domain, str.c_str());
		}

	}

	static std::unordered_map<MonoType*, std::function<bool(Entity)>> sEntityHasComponentFuncs;

#define TOAST_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Toast.InternalCalls::" #Name, Name)

#pragma region Log
	void Log_Trace(MonoObject* msg)
	{
		TOAST_TRACE(Utils::ConvertMonoObjectToCppChar(msg));
	}

	void Log_Info(MonoObject* msg)
	{
		TOAST_INFO(Utils::ConvertMonoObjectToCppChar(msg));
	}

	void Log_Warning(MonoObject* msg)
	{
		TOAST_WARN(Utils::ConvertMonoObjectToCppChar(msg));
	}

	void Log_Error(MonoObject* msg)
	{
		TOAST_ERROR(Utils::ConvertMonoObjectToCppChar(msg));
	}

	void Log_Critical(MonoObject* msg)
	{
		TOAST_CRITICAL(Utils::ConvertMonoObjectToCppChar(msg));
	}
#pragma endregion

#pragma region Input

	static bool Input_IsKeyPressed(KeyCode key)
	{
		return Input::IsKeyPressed(key);
	}

	bool Input_IsMouseButtonPressed(MouseCode button)
	{
		return Input::IsMouseButtonPressed(button);
	}

	bool Input_IsMouseButtonReleased(MouseCode button)
	{
		return Input::IsMouseButtonReleased(button);
	}

	void Input_GetMousePosition(DirectX::XMFLOAT2* outPos)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		auto viewportPos = scene->GetViewportPos();
		auto viewportSize = scene->GetViewportSize();

		DirectX::XMFLOAT2 outputPos;
		outputPos.x = Input::GetMousePosition().x - (float)std::get<0>(viewportPos);
		outputPos.y = Input::GetMousePosition().y - (float)std::get<1>(viewportPos);

		*outPos = outputPos;
	}

	float Input_GetMouseWheelDelta()
	{
		return Input::GetMouseWheelDelta();
	}

	void Input_SetMouseWheelDelta(float value)
	{
		Input::SetMouseWheelDelta(value);
	}

#pragma endregion

#pragma region Scene

	void Scene_GetRenderTargetSize(DirectX::XMFLOAT2* outSize)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		auto& size = scene->GetViewportSize();
		outSize->x = (float)std::get<0>(size);
		outSize->y = (float)std::get<1>(size);
	}

	void Scene_SetRenderColliders(bool renderColliders)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		scene->SetRenderColliders(renderColliders);
	}

	bool Scene_GetRenderColliders()
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		return scene->GetRenderColliders();
	}

	void Scene_SetTimeScale(float value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		scene->SetTimeScale(value);
	}

	float Scene_GetTimeScale()
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		return scene->GetTimeScale();
	}

	uint64_t Scene_AddPrefab(MonoString* name)
	{
		char* nameCStr = mono_string_to_utf8(name);

		std::string nameStr = std::string(nameCStr);

		Scene* scene = ScriptEngine::GetSceneContext();

		Entity& prefabEntity = scene->AddPrefab(nameStr);

		TOAST_CORE_CRITICAL("Adding Prefab!");

		return prefabEntity.GetUUID();
	}

	static MonoArray* Scene_GetEntitiesWithPrefab(MonoString* prefabNameMono)
	{
		char* prefabNameCStr = mono_string_to_utf8(prefabNameMono);
		std::string prefabName = std::string(prefabNameCStr);

		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		std::vector<Entity> entities = scene->GetEntitiesWithPrefab(prefabName);

		mono_free(prefabNameCStr);

		MonoDomain* domain = mono_domain_get();
		MonoClass* ulongClass = mono_get_uint64_class();       
		MonoArray* resultArray = mono_array_new(domain, ulongClass, static_cast<uint32_t>(entities.size()));

		for (uint32_t i = 0; i < entities.size(); ++i)
			mono_array_set(resultArray, uint64_t, i, entities[i].GetUUID());

		return resultArray;
	}

	static void Scene_RequestSceneChange(MonoString* sceneNameMono)
	{
		std::string sceneName = Toast::Utils::ConvertMonoStringToCppString(sceneNameMono);

		// Queue onto main thread (safe even if scripts run on main thread; avoids re-entrancy)
		Toast::Application::Get().SubmitToMainThread([sceneName]()
			{
				auto* provider = Toast::Application::Get().GetSceneProvider();
				if (!provider)
				{
					TOAST_CORE_WARN("Scene_RequestSceneChange('%s') failed: no scene provider.", sceneName.c_str());
					return;
				}

				provider->RequestSceneChange(sceneName);
			});
	}

	static bool Scene_GetWorldPositionUnderCursor(double* outX, double* outY, double* outZ)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Vector3 result;
		bool valid = scene->GetWorldPositionUnderCursor(result);
		*outX = result.x; *outY = result.y; *outZ = result.z;
		return valid;
	}

	static uint64_t Scene_GetHoveredEntity()
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");

		entt::entity handle = scene->GetHoveredEntity();
		if (handle == entt::null)
			return 0;

		Entity entity{ handle, scene };
		return entity.GetUUID();
	}

#pragma endregion

#pragma region Selection

	static void Selection_Clear()
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		scene->GetSelectionSystem().ClearSelection();
	}

	static uint32_t Selection_GetCount()
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		return (uint32_t)scene->GetSelectionSystem().GetSelected().size();
	}

	static uint64_t Selection_GetAt(uint32_t index)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		const auto& selected = scene->GetSelectionSystem().GetSelected();
		if (index >= selected.size()) return 0;
		Entity e = selected[index];   // copy — Entity is two pointers, trivially copyable
		return e.GetUUID();
	}

#pragma endregion

#pragma region Physics Engine

	static float PhysicsEngine_GetAltitude(UUID entityID, bool ignoreWorldTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		return scene->GetAltitude(entity, ignoreWorldTranslation);
	}

	static float PhysicsEngine_GetAltitudeAtWorldPos(DirectX::XMFLOAT3 worldPos)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		double radialDist;
		Vector3 groundNormal;
		return scene->GetAltitudeAtWorldPos(worldPos, radialDist, groundNormal);
	}

	static void PhysicsEngine_ApplyLinearImpulse(UUID entityID, DirectX::XMFLOAT3 impulse)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& physicsEngine = scene->GetPhysicsEngine();

		auto& rbc = entity.GetComponent<RigidBodyComponent>();

		physicsEngine->ApplyLinearImpulse(rbc, impulse);
	}

	static void PhysicsEngine_ApplyLinearImpulseAtPoint(UUID entityID, DirectX::XMFLOAT3 impulse, DirectX::XMFLOAT3 worldPoint)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& physicsEngine = scene->GetPhysicsEngine();
		auto& rbc = entity.GetComponent<RigidBodyComponent>();
		auto& tc = entity.GetComponent<TransformComponent>();

		// Compute CoM in world space
		DirectX::XMVECTOR totalQuat = tc.GetTotalRotationQuaternion();
		DirectX::XMFLOAT3 CoM = { (float)rbc.CenterOfMass.x, (float)rbc.CenterOfMass.y, (float)rbc.CenterOfMass.z};
		DirectX::XMVECTOR comLocal = DirectX::XMLoadFloat3(&CoM);
		DirectX::XMVECTOR comWorld = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&tc.Translation),DirectX::XMVector3Rotate(comLocal, totalQuat));

		DirectX::XMFLOAT3 comWorldF3;
		DirectX::XMStoreFloat3(&comWorldF3, comWorld);

		physicsEngine->ApplyLinearImpulseAtPoint(rbc, impulse, worldPoint, comWorldF3);
	}

#pragma endregion

#pragma region Script

	static MonoObject* Script_GetInstance(UUID entityID)
	{
		return ScriptEngine::GetManagedInstance(entityID);
	}

#pragma endregion

#pragma region Planet

	static void Planet_GetTranslation(DirectX::XMFLOAT3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		*outTranslation = scene->GetPlanet()->GetTranslation();
	}

	static void Planet_SetTranslation(DirectX::XMFLOAT3* translation)
	{
		; // TODO IMPLEMENT THIS
	}

	static float Planet_GetGravity()
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		return scene->GetPlanet()->GetGravityConstant();
	}

#pragma endregion

#pragma region Entity

	static bool Entity_HasComponent(UUID entityID, MonoReflectionType* componentType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		MonoType* managedType = mono_reflection_type_get_type(componentType);
		TOAST_CORE_ASSERT(sEntityHasComponentFuncs.find(managedType) != sEntityHasComponentFuncs.end(), "");
		return sEntityHasComponentFuncs.at(managedType)(entity);
	}

	static uint64_t Entity_FindEntityByName(MonoString* name)
	{
		char* nameCStr = mono_string_to_utf8(name);

		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindEntityByName(nameCStr);
		mono_free(nameCStr);

		if (!entity)
		{
			std::string& nameStr = Utils::ConvertMonoStringToCppString(name);
			TOAST_CORE_CRITICAL("Entity '%s' not found!", nameStr.c_str());
			return 0;
		}

		return entity.GetUUID();
	}

	static uint64_t Entity_FindChildEntityByName(MonoString* parentName, MonoString* childName)
	{
		char* parentNameCStr = mono_string_to_utf8(parentName);
		char* childNameCStr = mono_string_to_utf8(childName);

		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindChildEntityByName(parentNameCStr, childNameCStr);
		mono_free(parentNameCStr);
		mono_free(childNameCStr);

		if (!entity)
		{
			std::string& parentNameStr = Utils::ConvertMonoStringToCppString(parentName);
			std::string& childNameStr = Utils::ConvertMonoStringToCppString(parentName);

			TOAST_CORE_CRITICAL("Child to Entity '%s' with name '%s' not found!", parentNameStr.c_str(), childNameStr.c_str());
			return 0;
		}

		return entity.GetUUID();
	}

	static uint64_t Entity_FindParentEntity(UUID childID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindParentEntity(childID);

		if (!entity)
			return 0;

		return entity.GetUUID();
	}

	static uint64_t Entity_FindDecententByName(uint64_t parentID, MonoString* name)
	{
		char* nameCStr = mono_string_to_utf8(name);

		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity parent = scene->FindEntityByUUID(parentID);

		if (!parent)
		{
			TOAST_CORE_CRITICAL("Entity_FindDecententByName: parent entity not found (UUID: %llu)!",
				(unsigned long long)parentID);
			return 0;
		}

		Entity found = scene->FindDescendantByName(parent, nameCStr);

		mono_free(nameCStr);
		if (!found)
		{
			std::string& childNameStr = Utils::ConvertMonoStringToCppString(name);
			TOAST_CORE_CRITICAL("Entity_FindDecententByName: descendant '%s' not found under parent '%s' (UUID: %llu)!", childNameStr.c_str(), parent.GetComponent<TagComponent>().Tag.c_str(), parentID);
			return 0;
		}
		return found.GetUUID();
	}

	static void Entity_Select(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindEntityByUUID(entityID);
		TOAST_CORE_ASSERT(entity, "");
		scene->GetSelectionSystem().Select(entity);
	}

	static void Entity_Deselect(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindEntityByUUID(entityID);
		TOAST_CORE_ASSERT(entity, "");
		scene->GetSelectionSystem().Deselect(entity);
	}

	static void Entity_SelectExclusive(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindEntityByUUID(entityID);
		TOAST_CORE_ASSERT(entity, "");
		scene->GetSelectionSystem().SelectExclusive(entity);
	}

	static bool Entity_IsSelected(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindEntityByUUID(entityID);
		TOAST_CORE_ASSERT(entity, "");
		return scene->GetSelectionSystem().IsSelected(entity);
	}

	static void Entity_MoveTo(UUID entityID, double targetX, double targetY, double targetZ, float speed)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		if (!entity || !entity.HasComponent<MoveableComponent>()) return;
		auto& cfg = entity.GetComponent<MoveableComponent>();
		if (!cfg.IsActive) return;

		Vector3 worldTranslation = scene->GetMainCamera()->GetWorldTranslation();

		Vector3 target = Vector3(targetX, targetY, targetZ) - worldTranslation;
		Planet& planet = *scene->GetPlanet();
		Vector3 normal = Vector3::Normalize(target - Vector3(planet.GetTranslation()));

		auto& cmd = entity.AddOrReplaceComponent<MoveCommandComponent>();
		cmd.TargetWorldPos = target;
		cmd.TargetSurfaceNormal = normal;
		cmd.Speed = speed;
		cmd.MarkerElapsed = 0.0f;
	}

	static bool Entity_IsSelectable(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		if (!entity) return false;
		return entity.HasComponent<MeshComponent>();
	}

	static void Entity_Unparent(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		if (entity) scene->UnparentEntity(entity);
	}

#pragma endregion

#pragma region Tag Component

	MonoString* TagComponent_GetTag(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		std::string tag;

		if (!entity.HasComponent<TagComponent>())
			tag = "Unknown";
		else 
		{
			auto& component = entity.GetComponent<TagComponent>();
			tag = component.Tag;
		}

		return Utils::ConvertCppStringToMonoString(mono_domain_get(), tag);
	}

	void TagComponent_SetTag(uint64_t entityID, MonoString* tag)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<TagComponent>();
		std::string& tagStr = Utils::ConvertMonoStringToCppString(tag);
		component.Tag = tagStr;
	}

#pragma endregion

#pragma region Transform Component

	static void TransformComponent_GetTranslation(UUID entityID, DirectX::XMFLOAT3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outTranslation = entity.GetComponent<TransformComponent>().Translation;
	}

	static void TransformComponent_SetTranslation(UUID entityID, DirectX::XMFLOAT3* translation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().Translation = *translation;
	}

	static void TransformComponent_GetRotation(UUID entityID, DirectX::XMFLOAT4* outRotation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& tc = entity.GetComponent<TransformComponent>();
		DirectX::XMVECTOR q = tc.GetTotalRotationQuaternion();

		DirectX::XMStoreFloat4(outRotation, q);
	}

	static void TransformComponent_SetRotation(UUID entityID, DirectX::XMFLOAT4* rotation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		tc.RotationQuaternion = *rotation;
		tc.IsDirty = true;
	}

	static void TransformComponent_GetRotationQuaternion(UUID entityID, DirectX::XMFLOAT4* outQuat)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outQuat = entity.GetComponent<TransformComponent>().RotationQuaternion;
	}

	static void TransformComponent_SetRotationQuaternion(UUID entityID, DirectX::XMFLOAT4* quat)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		tc.RotationQuaternion = *quat;
		tc.IsDirty = true;
	}

	static void TransformComponent_GetPitch(UUID entityID, float* outPitch)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outPitch = entity.GetComponent<TransformComponent>().RotationEulerAngles.x;
	}

	static void TransformComponent_SetPitch(UUID entityID, float* pitch)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().RotationEulerAngles.x = *pitch;
	}

	static void TransformComponent_GetYaw(UUID entityID, float* outYaw)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outYaw = entity.GetComponent<TransformComponent>().RotationEulerAngles.y;
	}

	static void TransformComponent_SetYaw(UUID entityID, float* yaw)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().RotationEulerAngles.y = *yaw;
	}

	static void TransformComponent_GetRoll(UUID entityID, float* outRoll)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outRoll = entity.GetComponent<TransformComponent>().RotationEulerAngles.z;
	}

	static void TransformComponent_SetRoll(UUID entityID, float* roll)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().RotationEulerAngles.z = *roll;
	}

	static void TransformComponent_GetScale(UUID entityID, DirectX::XMFLOAT3* outScale)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outScale = entity.GetComponent<TransformComponent>().Scale;
	}

	static void TransformComponent_SetScale(UUID entityID, DirectX::XMFLOAT3* scale)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().Scale = *scale;
	}

	static void TransformComponent_GetTransform(UUID entityID, DirectX::XMMATRIX* outTransform)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outTransform = entity.GetComponent<TransformComponent>().GetTransform();
	}

	static void TransformComponent_Rotate(UUID entityID, DirectX::XMFLOAT3* rotationAxis, float angleDeg)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		auto& tc = scene->FindEntityByUUID(entityID).GetComponent<TransformComponent>();
		DirectX::XMVECTOR qCur = DirectX::XMLoadFloat4(&tc.RotationQuaternion);

		DirectX::XMVECTOR axis = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(rotationAxis));
		DirectX::XMVECTOR qInc = DirectX::XMQuaternionRotationAxis(axis, DirectX::XMConvertToRadians(angleDeg));
		qCur = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(qCur, qInc));

		DirectX::XMStoreFloat4(&tc.RotationQuaternion, qCur);
	}

	static void TransformComponent_RotateAroundPoint(UUID entityID, DirectX::XMFLOAT3* point, DirectX::XMFLOAT3* rotationAxis, float angle)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		DirectX::XMVECTOR vectorPoint = DirectX::XMLoadFloat3(point);

		DirectX::XMVECTOR translatedObject = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity.GetComponent<TransformComponent>().Translation), vectorPoint);

		DirectX::XMVECTOR rotQuaternion = DirectX::XMQuaternionRotationAxis(DirectX::XMLoadFloat3(rotationAxis), DirectX::XMConvertToRadians(angle));

		translatedObject = DirectX::XMVector3Transform(translatedObject, DirectX::XMMatrixRotationQuaternion(rotQuaternion));

		translatedObject = DirectX::XMVectorAdd(translatedObject, vectorPoint);

		DirectX::XMStoreFloat3(&entity.GetComponent<TransformComponent>().Translation, translatedObject);
	}

	static void TransformComponent_SetAngularSpeed(UUID entityID, float speed)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		tc.AngularSpeed = speed;
	}

	static void TransformComponent_GetAngularSpeed(UUID entityID, float* outSpeed)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		*outSpeed = tc.AngularSpeed;
	}

	static void TransformComponent_SetIsRotating(UUID entityID, bool rotating)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		tc.IsRotating = rotating;
	}

	static bool TransformComponent_GetIsRotating(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		return tc.IsRotating;
	}

	static void TransformComponent_SetTargetRotation(UUID entityID, float pitchDeg, float yawDeg, float rollDeg)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();

		DirectX::XMVECTOR qTarget = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(pitchDeg), DirectX::XMConvertToRadians(yawDeg), DirectX::XMConvertToRadians(rollDeg));

		DirectX::XMStoreFloat4(&tc.TargetRotationQuaternion, DirectX::XMQuaternionNormalize(qTarget));
		tc.IsRotating = true;
	} 

	static void TransformComponent_SetTargetRotationDelta(UUID entityID, float pitchDeg, float yawDeg, float rollDeg)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();

		// Current absolute orientation
		DirectX::XMVECTOR current = tc.GetTotalRotationQuaternion();

		// The delta rotation to apply
		DirectX::XMVECTOR delta = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(pitchDeg), DirectX::XMConvertToRadians(yawDeg), DirectX::XMConvertToRadians(rollDeg));

		// Target = current composed with delta. Order matters (see note).
		DirectX::XMVECTOR target = DirectX::XMQuaternionMultiply(delta, current);  // or (current, delta)
		DirectX::XMStoreFloat4(&tc.TargetRotationQuaternion, DirectX::XMQuaternionNormalize(target));
		tc.IsRotating = true;
	}

	static bool TransformComponent_HasReachedTargetRotation(UUID entityID, float thresholdDeg = 0.5f)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();

		DirectX::XMVECTOR qCurrent = tc.GetTotalRotationQuaternion();
		DirectX::XMVECTOR qTarget = DirectX::XMLoadFloat4(&tc.TargetRotationQuaternion);

		float dot = std::abs(DirectX::XMVectorGetX(DirectX::XMVector4Dot(qCurrent, qTarget)));

		float dotThreshold = std::cos(DirectX::XMConvertToRadians(thresholdDeg) * 0.5f);
		return dot >= dotThreshold;
	}

	static void TransformComponent_GetWorldUp(UUID entityID, DirectX::XMFLOAT3* outUp)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();

		// Get the combined rotation: euler * quaternion (matches GetRotation() order)
		DirectX::XMVECTOR totalQuat = tc.GetTotalRotationQuaternion();

		// Rotate the default up axis (0, 1, 0) by the total rotation
		DirectX::XMVECTOR defaultUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR worldUp = DirectX::XMVector3Rotate(defaultUp, totalQuat);

		DirectX::XMStoreFloat3(outUp, worldUp);
	}

	static void TransformComponent_GetWorldForward(UUID entityID, DirectX::XMFLOAT3* outForward)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();

		// Rotate default forward (0, 0, 1) by total rotation
		DirectX::XMVECTOR defaultForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		DirectX::XMVECTOR worldForward = DirectX::XMVector3Rotate(defaultForward, tc.GetTotalRotationQuaternion());

		DirectX::XMStoreFloat3(outForward, worldForward);
	}

	static void TransformComponent_GetWorldRight(UUID entityID, DirectX::XMFLOAT3* outRight)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();

		DirectX::XMVECTOR totalQuat = tc.GetTotalRotationQuaternion();
		DirectX::XMVECTOR defaultRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR worldRight = DirectX::XMVector3Rotate(defaultRight, totalQuat);

		DirectX::XMStoreFloat3(outRight, worldRight);
	}

	static void TransformComponent_SetTargetTranslation(UUID entityID, DirectX::XMFLOAT3* target) 
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		tc.TargetTranslation = *target;
		tc.IsTranslating = true;
	}

	static void TransformComponent_SetTranslationSpeed(UUID entityID, float speed)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().TranslationSpeed = speed;
	}

	static void TransformComponent_GetTranslationSpeed(UUID entityID, float* outSpeed)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outSpeed = entity.GetComponent<TransformComponent>().TranslationSpeed;
	}

	static void TransformComponent_SetIsTranslating(UUID entityID, bool translating)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().IsTranslating = translating;
	}

	static bool TransformComponent_GetIsTranslating(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		return entity.GetComponent<TransformComponent>().IsTranslating;
	}

	static bool TransformComponent_HasReachedTargetTranslation(UUID entityID, float threshold = 0.05f)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& tc = entity.GetComponent<TransformComponent>();
		DirectX::XMVECTOR cur = DirectX::XMLoadFloat3(&tc.Translation);
		DirectX::XMVECTOR tgt = DirectX::XMLoadFloat3(&tc.TargetTranslation);
		float d = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(tgt, cur)));
		return d <= threshold;
	}

#pragma endregion

#pragma region Mesh Component

	static void MeshComponent_GeneratePlanet(uint64_t entityID, DirectX::XMFLOAT3* cameraPos, DirectX::XMMATRIX* cameraTransform)
	{
		//Scene* scene = ScriptEngine::GetSceneContext();
		//auto sceneSettings = scene->GetSettings();
		//TOAST_CORE_ASSERT(scene, "No active scene!");
		//const auto& entityMap = scene->GetEntityMap();
		//TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		//Entity entity = entityMap.at(entityID);
		//auto& pc = entity.GetComponent<PlanetComponent>();
		//auto& tc = entity.GetComponent<TransformComponent>();

		//DirectX::XMVECTOR cameraPosVector = { cameraPos->x, cameraPos->y, cameraPos->z };
		//DirectX::XMVECTOR cameraPosVector2, cameraRotVector, cameraScaleVector, cameraForward;
		//cameraForward = { 0.0f, 0.0f, 1.0f };
		//DirectX::XMMatrixDecompose(&cameraScaleVector, &cameraRotVector, &cameraPosVector2, *cameraTransform);
		//cameraForward = DirectX::XMVector3Rotate(cameraForward, cameraRotVector);
		////PlanetSystem::GeneratePlanet(scene->GetFrustum(), tc.GetTransform(), pc.Mesh->mVertices, pc.Mesh->GetPlanetPatches(), pc.DistanceLUT, pc.FaceLevelDotLUT, pc.HeightMultLUT, cameraPosVector, cameraForward, pc.Subdivisions, pc.PlanetData.radius, sceneSettings.BackfaceCulling, sceneSettings.FrustumCulling);

		//pc.RenderMesh->InvalidatePlanet();
	}

	static void MeshComponent_PlayAnimation(uint64_t entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& mc = entity.GetComponent<MeshComponent>();
		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(mc.MeshHandle);
		if (!mesh)
			return;

		if (!mesh->HasAnimation(nameStr))
		{
			TOAST_CORE_WARN("Animation '%s' not found for entity %llu", nameStr.c_str(), entityID);
			return;
		}

		mc.Playbacks[nameStr].Play();
		TOAST_CORE_INFO("Playing animation '%s'", nameStr.c_str());
	}

	static void MeshComponent_PlayReverseAnimation(uint64_t entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& mc = entity.GetComponent<MeshComponent>();
		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(mc.MeshHandle);
		if (!mesh)
			return;

		if (!mesh->HasAnimation(nameStr))
		{
			TOAST_CORE_WARN("Animation '%s' not found for entity %llu", nameStr.c_str(), entityID);
			return;
		}

		mc.Playbacks[nameStr].PlayReverse();
		TOAST_CORE_INFO("Playing animation reversed '%s'", nameStr.c_str());
	}

	static float MeshComponent_StopAnimation(uint64_t entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& mc = entity.GetComponent<MeshComponent>();
		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		auto it = mc.Playbacks.find(nameStr);
		if (it == mc.Playbacks.end()) return 0.0f;

		float timeElapsed = it->second.TimeElapsed;
		it->second.IsActive = false;
		it->second.TimeElapsed = 0.0f;
		return timeElapsed;
	}

	static float MeshComponent_GetAnimationTimeElapsed(uint64_t entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& mc = entity.GetComponent<MeshComponent>();
		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		auto it = mc.Playbacks.find(nameStr);
		return it == mc.Playbacks.end() ? 0.0f : it->second.TimeElapsed;
	}

	static bool MeshComponent_IsAnimationComplete(UUID entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		if (!entity.HasComponent<MeshComponent>()) return false;
		char* cstr = mono_string_to_utf8(name);
		std::string animName(cstr);
		mono_free(cstr);

		auto& mc = entity.GetComponent<MeshComponent>();
		auto it = mc.Playbacks.find(animName);
		if (it == mc.Playbacks.end())
			return false;

		return it->second.HasPlayed && !it->second.IsActive;
	}

	static float MeshComponent_GetDurationAnimation(uint64_t entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& mc = entity.GetComponent<MeshComponent>();
		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(mc.MeshHandle);
		if (!mesh)
			return 0.0f;

		return mesh->GetAnimationDuration(nameStr);
	}

#pragma endregion

#pragma region Camera Component

	float CameraComponent_GetFarClip(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		return component.Camera.GetFarClip();
	}

	void CameraComponent_SetFarClip(uint64_t entityID, float inFarClip)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		component.Camera.SetFarClip(inFarClip);
	}

	float CameraComponent_GetNearClip(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		return component.Camera.GetNearClip();
	}

	void CameraComponent_SetNearClip(uint64_t entityID, float inNearClip)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		component.Camera.SetNearClip(inNearClip);
	}

	static void CameraComponent_GetWorldTranslation(UUID entityID, DirectX::XMFLOAT3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outTranslation = entity.GetComponent<CameraComponent>().Camera.GetWorldTranslation();
	}

	static void CameraComponent_SetWorldTranslation(UUID entityID, DirectX::XMFLOAT3* worldTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<CameraComponent>().Camera.GetWorldTranslation() = *worldTranslation;
	}

#pragma endregion

#pragma region UI Panel Component

	bool UIPanelComponent_GetVisible(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<UIPanelComponent>();
		return component.Visible;
	}

	void UIPanelComponent_SetVisible(uint64_t entityID, bool value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<UIPanelComponent>();
		component.Visible = value;
	}

#pragma endregion

#pragma region UI Button Component

	void UIButtonComponent_GetColor(uint64_t entityID, DirectX::XMFLOAT4* outColor)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<UIButtonComponent>();
		*outColor = component.Color;
	}

	void UIButtonComponent_SetColor(uint64_t entityID, DirectX::XMFLOAT4* inColor)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<UIButtonComponent>();
		component.Color = *inColor;
	}

	bool UIButtonComponent_GetVisible(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<UIButtonComponent>();
		return component.Visible;
	}

	void UIButtonComponent_SetVisible(uint64_t entityID, bool value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<UIButtonComponent>();
		component.Visible = value;
	}

#pragma endregion

#pragma region UI Text Component

	MonoString* UITextComponent_GetText(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<UITextComponent>();

		std::string text = component.Text;

		return Utils::ConvertCppStringToMonoString(mono_domain_get(), text);
	}

	void UITextComponent_SetText(uint64_t entityID, MonoString* inText)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<UITextComponent>();

		std::string& textStr = Utils::ConvertMonoStringToCppString(inText);
		component.Text = textStr;
	}

#pragma endregion

#pragma region Rigid Body Component

	float RigidBodyComponent_GetAltitude(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<RigidBodyComponent>();
		return (float)component.Altitude;
	}

	void RigidBodyComponent_GetLinearVelocity(uint64_t entityID, DirectX::XMFLOAT3* outLinearVelocity)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<RigidBodyComponent>();

		DirectX::XMFLOAT3 linearVelocity = { (float)component.LinearVelocity.x, (float)component.LinearVelocity.y, (float)component.LinearVelocity.z };

		*outLinearVelocity = linearVelocity;
	}

	void RigidBodyComponent_GetAngularVelocity(uint64_t entityID, DirectX::XMFLOAT3* outAngularVelocity)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<RigidBodyComponent>();

		DirectX::XMFLOAT3 angularVelocity = { (float)component.AngularVelocity.x, (float)component.AngularVelocity.y, (float)component.AngularVelocity.z };

		*outAngularVelocity = angularVelocity;
	}

	static void RigidBodyComponent_SetMass(uint64_t entityID, float mass)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<RigidBodyComponent>();

		component.InvMass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
	}

	static float RigidBodyComponent_GetMass(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<RigidBodyComponent>();

		return (component.InvMass > 0.0f) ? (1.0f / component.InvMass) : 0.0f;
	}

	static void RigidBodyComponent_SetAngularDamping(uint64_t entityID, float damping)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<RigidBodyComponent>();

		component.AngularDamping = damping;
	}

	static float RigidBodyComponent_GetAngularDamping(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<RigidBodyComponent>();

		return component.AngularDamping;
	}

#pragma endregion

#pragma region Sphere Collider Component

	float SphereColliderComponent_GetAltitude(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<SphereColliderComponent>();

		return 0.0f;
	}

#pragma endregion

#pragma region Box Collider Component

	float BoxColliderComponent_GetAltitude(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<BoxColliderComponent>();

		double altitude = scene->GetPhysicsEngine()->GetAltitudeBoxCollider(entity);

		return static_cast<float>(altitude);
	}

	void BoxColliderComponent_GetSize(uint64_t entityID, DirectX::XMFLOAT3* outSize)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<BoxColliderComponent>();

		*outSize = { (float)component.Collider->mSize.x, (float)component.Collider->mSize.y, (float)component.Collider->mSize.z };
	}

	void BoxColliderComponent_SetSize(uint64_t entityID, DirectX::XMFLOAT3* size)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<BoxColliderComponent>();

		component.Collider->mSize = Vector3(*size);
		component.IsDirty = true;
	}

	void BoxColliderComponent_GetOffset(uint64_t entityID, DirectX::XMFLOAT3* outOffset)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<BoxColliderComponent>();

		*outOffset = { (float)component.Collider->mOffset.x, (float)component.Collider->mOffset.y, (float)component.Collider->mOffset.z };
	}

	void BoxColliderComponent_SetOffset(uint64_t entityID, DirectX::XMFLOAT3* offset)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<BoxColliderComponent>();

		component.Collider->mOffset = Vector3(*offset);
		component.IsDirty = true;
	}

#pragma endregion

#pragma region Particles Component

	void ParticlesComponent_SetEmitting(uint64_t entityID, bool value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		component.Emitting = value;
	}

	bool ParticlesComponent_GetEmitting(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		return component.Emitting;
	}

	void ParticlesComponent_SetMaxLifeTime(uint64_t entityID, float value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		component.MaxLifeTime = value;
	}

	float ParticlesComponent_GetMaxLifeTime(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		return component.MaxLifeTime;
	}

	void ParticlesComponent_SetSpawnDelay(uint64_t entityID, float value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		component.SpawnDelay = value;
	}

	float ParticlesComponent_GetSpawnDelay(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		return component.SpawnDelay;
	}

	void ParticlesComponent_SetStartIntensity(uint64_t entityID, float value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		component.StartIntensity = value;
	}

	float ParticlesComponent_GetStartIntensity(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ParticlesComponent>();

		return component.StartIntensity;
	}

	void ParticlesComponent_GetVelocity(uint64_t entityID, DirectX::XMFLOAT3* outVelocity)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<ParticlesComponent>();
		*outVelocity = component.Velocity;
	}

	void ParticlesComponent_SetVelocity(uint64_t entityID, DirectX::XMFLOAT3* velocity)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		auto& component = entity.GetComponent<ParticlesComponent>();
		component.Velocity = *velocity;
	}

#pragma endregion

#pragma region Script Component

	void* ScriptComponent_GetInstance(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& component = entity.GetComponent<ScriptComponent>();

		Ref<ScriptInstance> instance = ScriptEngine::GetEntityScriptInstance(entityID);
		TOAST_CORE_ASSERT(instance, "Script Instance not found!");

		return reinterpret_cast<void*>(instance->GetInstanceHandle());
	}

#pragma endregion

#pragma region Moveable Component

	bool MoveableComponent_GetIsActive(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& mc = entity.GetComponent<MoveableComponent>();

		return mc.IsActive;
	}

	void MoveableComponent_SetIsActive(uint64_t entityID, bool value)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);

		auto& mc = entity.GetComponent<MoveableComponent>();

		mc.IsActive = value;
	}

#pragma endregion

	template<typename Component>
	static void RegisterComponent()
	{
		std::string_view typeName = typeid(Component).name();
		size_t pos = typeName.find_last_of(':');
		std::string_view structName = typeName.substr(pos + 1);
		std::string managedTypename = "Toast.";
		managedTypename.append(structName);

		MonoType* managedType = mono_reflection_type_from_name(managedTypename.data(), ScriptEngine::GetCoreAssemblyImage());
		TOAST_CORE_ASSERT(managedType, "");
		sEntityHasComponentFuncs[managedType] = [](Entity entity) { return entity.HasComponent<Component>(); };
	}

	void ScriptGlue::RegisterComponents()
	{
		sEntityHasComponentFuncs.clear();
		RegisterComponent<TagComponent>();
		RegisterComponent<TransformComponent>();  
		RegisterComponent<MeshComponent>();
		RegisterComponent<CameraComponent>();
		RegisterComponent<UIPanelComponent>();
		RegisterComponent<UIButtonComponent>();
		RegisterComponent<UITextComponent>();
		RegisterComponent<RigidBodyComponent>();
		RegisterComponent<SphereColliderComponent>();
		RegisterComponent<BoxColliderComponent>();
		RegisterComponent<ParticlesComponent>();
		RegisterComponent<ScriptComponent>();
		RegisterComponent<MoveableComponent>();
	}

	void ScriptGlue::RegisterFunctions()
	{
		TOAST_ADD_INTERNAL_CALL(Log_Trace);
		TOAST_ADD_INTERNAL_CALL(Log_Info);
		TOAST_ADD_INTERNAL_CALL(Log_Warning);
		TOAST_ADD_INTERNAL_CALL(Log_Error);
		TOAST_ADD_INTERNAL_CALL(Log_Critical);

		TOAST_ADD_INTERNAL_CALL(Input_IsKeyPressed);
		TOAST_ADD_INTERNAL_CALL(Input_IsMouseButtonPressed);
		TOAST_ADD_INTERNAL_CALL(Input_IsMouseButtonReleased);
		TOAST_ADD_INTERNAL_CALL(Input_GetMousePosition);
		TOAST_ADD_INTERNAL_CALL(Input_GetMouseWheelDelta);
		TOAST_ADD_INTERNAL_CALL(Input_SetMouseWheelDelta);

		TOAST_ADD_INTERNAL_CALL(PhysicsEngine_GetAltitude);
		TOAST_ADD_INTERNAL_CALL(PhysicsEngine_GetAltitudeAtWorldPos);
		TOAST_ADD_INTERNAL_CALL(PhysicsEngine_ApplyLinearImpulse);
		TOAST_ADD_INTERNAL_CALL(PhysicsEngine_ApplyLinearImpulseAtPoint);

		TOAST_ADD_INTERNAL_CALL(Scene_GetRenderTargetSize);
		TOAST_ADD_INTERNAL_CALL(Scene_GetRenderColliders);
		TOAST_ADD_INTERNAL_CALL(Scene_SetRenderColliders);
		TOAST_ADD_INTERNAL_CALL(Scene_GetTimeScale);
		TOAST_ADD_INTERNAL_CALL(Scene_SetTimeScale);
		TOAST_ADD_INTERNAL_CALL(Scene_AddPrefab);
		TOAST_ADD_INTERNAL_CALL(Scene_GetEntitiesWithPrefab);
		TOAST_ADD_INTERNAL_CALL(Scene_RequestSceneChange);
		TOAST_ADD_INTERNAL_CALL(Scene_GetWorldPositionUnderCursor);
		TOAST_ADD_INTERNAL_CALL(Scene_GetHoveredEntity);

		TOAST_ADD_INTERNAL_CALL(Selection_Clear);
		TOAST_ADD_INTERNAL_CALL(Selection_GetCount);
		TOAST_ADD_INTERNAL_CALL(Selection_GetAt);

		TOAST_ADD_INTERNAL_CALL(Planet_GetTranslation);
		TOAST_ADD_INTERNAL_CALL(Planet_SetTranslation);
		TOAST_ADD_INTERNAL_CALL(Planet_GetGravity);

		TOAST_ADD_INTERNAL_CALL(Script_GetInstance);

		TOAST_ADD_INTERNAL_CALL(Entity_HasComponent);
		TOAST_ADD_INTERNAL_CALL(Entity_FindEntityByName);
		TOAST_ADD_INTERNAL_CALL(Entity_FindChildEntityByName);
		TOAST_ADD_INTERNAL_CALL(Entity_FindParentEntity);
		TOAST_ADD_INTERNAL_CALL(Entity_FindDecententByName);
		TOAST_ADD_INTERNAL_CALL(Entity_Select);
		TOAST_ADD_INTERNAL_CALL(Entity_Deselect);
		TOAST_ADD_INTERNAL_CALL(Entity_SelectExclusive);
		TOAST_ADD_INTERNAL_CALL(Entity_IsSelected);
		TOAST_ADD_INTERNAL_CALL(Entity_MoveTo);
		TOAST_ADD_INTERNAL_CALL(Entity_IsSelectable);
		TOAST_ADD_INTERNAL_CALL(Entity_Unparent);

		TOAST_ADD_INTERNAL_CALL(TagComponent_GetTag);
		TOAST_ADD_INTERNAL_CALL(TagComponent_SetTag);

		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetRotation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetRotation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetRotationQuaternion);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetRotationQuaternion);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetTargetTranslation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetTranslationSpeed);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetTranslationSpeed);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetIsTranslating);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetIsTranslating);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_HasReachedTargetTranslation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetPitch);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetPitch);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetYaw);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetYaw);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetRoll);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetRoll);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetScale);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetScale);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetTransform);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_Rotate);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_RotateAroundPoint);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetAngularSpeed);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetAngularSpeed);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetIsRotating);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetIsRotating);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetTargetRotation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetTargetRotationDelta);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_HasReachedTargetRotation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetWorldUp);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetWorldForward);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetWorldRight);	

		TOAST_ADD_INTERNAL_CALL(MeshComponent_GeneratePlanet);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_PlayAnimation);		
		TOAST_ADD_INTERNAL_CALL(MeshComponent_PlayReverseAnimation);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_StopAnimation);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_GetAnimationTimeElapsed);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_IsAnimationComplete);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_GetDurationAnimation);

		TOAST_ADD_INTERNAL_CALL(CameraComponent_GetFarClip);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_SetFarClip);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_GetNearClip);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_SetNearClip);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_GetWorldTranslation);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_SetWorldTranslation);

		TOAST_ADD_INTERNAL_CALL(UIPanelComponent_GetVisible);
		TOAST_ADD_INTERNAL_CALL(UIPanelComponent_SetVisible);

		TOAST_ADD_INTERNAL_CALL(UIButtonComponent_GetColor);
		TOAST_ADD_INTERNAL_CALL(UIButtonComponent_SetColor);
		TOAST_ADD_INTERNAL_CALL(UIButtonComponent_GetVisible);
		TOAST_ADD_INTERNAL_CALL(UIButtonComponent_SetVisible);

		TOAST_ADD_INTERNAL_CALL(UITextComponent_GetText);
		TOAST_ADD_INTERNAL_CALL(UITextComponent_SetText);
		
		TOAST_ADD_INTERNAL_CALL(RigidBodyComponent_GetAltitude);
		TOAST_ADD_INTERNAL_CALL(RigidBodyComponent_GetLinearVelocity);
		TOAST_ADD_INTERNAL_CALL(RigidBodyComponent_GetAngularVelocity);
		TOAST_ADD_INTERNAL_CALL(RigidBodyComponent_SetMass);
		TOAST_ADD_INTERNAL_CALL(RigidBodyComponent_GetMass);
		TOAST_ADD_INTERNAL_CALL(RigidBodyComponent_SetAngularDamping);
		TOAST_ADD_INTERNAL_CALL(RigidBodyComponent_GetAngularDamping);

		TOAST_ADD_INTERNAL_CALL(SphereColliderComponent_GetAltitude);

		TOAST_ADD_INTERNAL_CALL(BoxColliderComponent_GetAltitude);
		TOAST_ADD_INTERNAL_CALL(BoxColliderComponent_SetSize);
		TOAST_ADD_INTERNAL_CALL(BoxColliderComponent_GetSize);
		TOAST_ADD_INTERNAL_CALL(BoxColliderComponent_SetOffset);
		TOAST_ADD_INTERNAL_CALL(BoxColliderComponent_GetOffset);

		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_GetEmitting);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_SetEmitting);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_GetMaxLifeTime);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_SetMaxLifeTime);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_GetSpawnDelay);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_SetSpawnDelay);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_GetStartIntensity);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_SetStartIntensity);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_GetVelocity);
		TOAST_ADD_INTERNAL_CALL(ParticlesComponent_SetVelocity);

		TOAST_ADD_INTERNAL_CALL(ScriptComponent_GetInstance);

		TOAST_ADD_INTERNAL_CALL(MoveableComponent_GetIsActive);
		TOAST_ADD_INTERNAL_CALL(MoveableComponent_SetIsActive);
	}

}