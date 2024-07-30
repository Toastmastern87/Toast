#include "tpch.h"
#include "ScriptGlue.h"
#include "ScriptEngine.h"

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"

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

	void Input_GetMousePosition(DirectX::XMFLOAT2* outPos)
	{
		*outPos = Input::GetMousePosition();
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

#pragma endregion

#pragma region Script

	static MonoObject* Script_GetInstance(UUID entityID)
	{
		return ScriptEngine::GetManagedInstance(entityID);
	}

#pragma endregion

#pragma region Entity

	static bool Entity_HasComponent(UUID entityID, MonoReflectionType* componentType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "");
		Entity entity = scene->FindEntityByUUID(entityID);
		TOAST_CORE_ASSERT(entity, "");

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
			TOAST_CORE_CRITICAL("Entity not found!");
			return 0;
		}

		return entity.GetUUID();
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
		auto& component = entity.GetComponent<TagComponent>();
		std::string tag = component.Tag;

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

	static void TransformComponent_GetRotation(UUID entityID, DirectX::XMFLOAT3* outRotation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		*outRotation = entity.GetComponent<TransformComponent>().RotationEulerAngles;
	}

	static void TransformComponent_SetRotation(UUID entityID, DirectX::XMFLOAT3* rotation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().RotationEulerAngles = *rotation;
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

	static void TransformComponent_Rotate(UUID entityID, DirectX::XMFLOAT3* rotationAxis, float angle)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		Entity entity = scene->FindEntityByUUID(entityID);
		DirectX::XMVECTOR rotQuaternion = DirectX::XMQuaternionRotationAxis(DirectX::XMLoadFloat3(rotationAxis), DirectX::XMConvertToRadians(angle));
		DirectX::XMStoreFloat4(&entity.GetComponent<TransformComponent>().RotationQuaternion, DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&entity.GetComponent<TransformComponent>().RotationQuaternion), rotQuaternion)));
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

#pragma endregion

#pragma region Mesh Component

	static void MeshComponent_GeneratePlanet(uint64_t entityID, DirectX::XMFLOAT3* cameraPos, DirectX::XMMATRIX* cameraTransform)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		auto sceneSettings = scene->GetSettings();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& pc = entity.GetComponent<PlanetComponent>();
		auto& tc = entity.GetComponent<TransformComponent>();

		DirectX::XMVECTOR cameraPosVector = { cameraPos->x, cameraPos->y, cameraPos->z };
		DirectX::XMVECTOR cameraPosVector2, cameraRotVector, cameraScaleVector, cameraForward;
		cameraForward = { 0.0f, 0.0f, 1.0f };
		DirectX::XMMatrixDecompose(&cameraScaleVector, &cameraRotVector, &cameraPosVector2, *cameraTransform);
		cameraForward = DirectX::XMVector3Rotate(cameraForward, cameraRotVector);
		//PlanetSystem::GeneratePlanet(scene->GetFrustum(), tc.GetTransform(), pc.Mesh->mVertices, pc.Mesh->GetPlanetPatches(), pc.DistanceLUT, pc.FaceLevelDotLUT, pc.HeightMultLUT, cameraPosVector, cameraForward, pc.Subdivisions, pc.PlanetData.radius, sceneSettings.BackfaceCulling, sceneSettings.FrustumCulling);

		pc.RenderMesh->InvalidatePlanet();
	}

	static void MeshComponent_PlayAnimation(uint64_t entityID, MonoString* name, float startTime)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		auto sceneSettings = scene->GetSettings();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);

		auto& mc = entity.GetComponent<MeshComponent>();

		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		//TOAST_CORE_INFO("Getting animation: %s", nameStr.c_str());

		for (auto& submesh : mc.MeshObject->GetSubmeshes())
		{
			if (submesh.IsAnimated) 
			{
				if (submesh.Animations.find(nameStr) != submesh.Animations.end())
					submesh.Animations[nameStr]->Play(startTime);
			}
		}
	}

	static float MeshComponent_StopAnimation(uint64_t entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		auto sceneSettings = scene->GetSettings();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);

		auto& mc = entity.GetComponent<MeshComponent>();

		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		//TOAST_CORE_INFO("Getting animation: %s", nameStr.c_str());

		for (auto& submesh : mc.MeshObject->GetSubmeshes())
		{
			if (submesh.IsAnimated)
			{
				if (submesh.Animations.find(nameStr) != submesh.Animations.end())
				{
					float elapsedTime = submesh.Animations[nameStr]->TimeElapsed;
					submesh.Animations[nameStr]->Reset();

					return elapsedTime;
				}	
			}
		}

		return 0.0f;
	}

	static float MeshComponent_GetDurationAnimation(uint64_t entityID, MonoString* name)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		auto sceneSettings = scene->GetSettings();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);

		auto& mc = entity.GetComponent<MeshComponent>();

		std::string& nameStr = Utils::ConvertMonoStringToCppString(name);

		//TOAST_CORE_INFO("Getting animation: %s", nameStr.c_str());

		for (auto& submesh : mc.MeshObject->GetSubmeshes())
		{
			if (submesh.IsAnimated)
			{
				if (submesh.Animations.find(nameStr) != submesh.Animations.end())
					return submesh.Animations[nameStr]->Duration;
			}
		}

		return 0.0f;
	}

#pragma endregion

#pragma region Camera Component

	float CameraComponent_GetFarClip(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		return component.Camera.GetFarClip();
	}

	void CameraComponent_SetFarClip(uint64_t entityID, float inFarClip)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		component.Camera.SetFarClip(inFarClip);
	}

	float CameraComponent_GetNearClip(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		return component.Camera.GetNearClip();
	}

	void CameraComponent_SetNearClip(uint64_t entityID, float inNearClip)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<CameraComponent>();

		component.Camera.SetNearClip(inNearClip);
	}

#pragma endregion

#pragma region Planet Component

	void PlanetComponent_GetRadius(uint64_t entityID, float* outRadius)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<PlanetComponent>();

		*outRadius = component.PlanetData.radius;
	}

	void PlanetComponent_GetSubdivisions(uint64_t entityID, int* outSubDivisions)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<PlanetComponent>();

		*outSubDivisions = component.Subdivisions;
	}

	MonoArray* PlanetComponent_GetDistanceLUT(uint64_t entityID)
	{
		MonoArray* outDistanceLUT;

		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<PlanetComponent>();

		outDistanceLUT = mono_array_new(mono_domain_get(), mono_get_double_class(), component.DistanceLUT.size());

		for (int i = 0; i < component.DistanceLUT.size(); i++)
			mono_array_set(outDistanceLUT, double, i, component.DistanceLUT[i]);

		return outDistanceLUT;
	}

	void PlanetComponent_GeneratePlanet(uint64_t entityID, DirectX::XMFLOAT3* cameraPos, DirectX::XMMATRIX* cameraTransform)
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
		//scene->InvalidateFrustum();

		//DirectX::XMMATRIX planetTransformNoScale = DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(tc.RotationEulerAngles.x), DirectX::XMConvertToRadians(tc.RotationEulerAngles.y), DirectX::XMConvertToRadians(tc.RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.RotationQuaternion)) * DirectX::XMMatrixTranslation(tc.Translation.x, tc.Translation.y, tc.Translation.z);

		////PlanetSystem::GeneratePlanet(scene->GetFrustum(), planetTransformNoScale, pc.Mesh->GetPlanetFaces(), pc.Mesh->GetPlanetPatches(), pc.DistanceLUT, pc.FaceLevelDotLUT, pc.HeightMultLUT, cameraPosVector, cameraForward, pc.Subdivisions, pc.PlanetData.radius, sceneSettings.BackfaceCulling, sceneSettings.FrustumCulling);

		//pc.RenderMesh->InvalidatePlanet();
	}

#pragma endregion

#pragma region UIButton Component

	void UIButtonComponent_GetColor(uint64_t entityID, DirectX::XMFLOAT4* outColor)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<UIButtonComponent>();
		*outColor = component.Button->GetColorF4();
	}

	void UIButtonComponent_SetColor(uint64_t entityID, DirectX::XMFLOAT4* inColor)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<UIButtonComponent>();
		component.Button->SetColor(*inColor);
	}

#pragma endregion

#pragma region UIText Component

	MonoString* UITextComponent_GetText(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<UITextComponent>();

		std::string text = component.Text->GetText();

		return Utils::ConvertCppStringToMonoString(mono_domain_get(), text);
	}

	void UITextComponent_SetText(uint64_t entityID, MonoString* inText)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		TOAST_CORE_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
		Entity entity = entityMap.at(entityID);
		auto& component = entity.GetComponent<UITextComponent>();

		std::string& textStr = Utils::ConvertMonoStringToCppString(inText);
		component.Text->SetText(textStr);
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
		RegisterComponent<PlanetComponent>();
		RegisterComponent<MeshComponent>();
		RegisterComponent<CameraComponent>();
		RegisterComponent<UIButtonComponent>();
		RegisterComponent<UITextComponent>();
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
		TOAST_ADD_INTERNAL_CALL(Input_GetMousePosition);
		TOAST_ADD_INTERNAL_CALL(Input_GetMouseWheelDelta);
		TOAST_ADD_INTERNAL_CALL(Input_SetMouseWheelDelta);

		TOAST_ADD_INTERNAL_CALL(Scene_GetRenderColliders);
		TOAST_ADD_INTERNAL_CALL(Scene_SetRenderColliders);
		TOAST_ADD_INTERNAL_CALL(Scene_GetTimeScale);
		TOAST_ADD_INTERNAL_CALL(Scene_SetTimeScale);

		TOAST_ADD_INTERNAL_CALL(Script_GetInstance);

		TOAST_ADD_INTERNAL_CALL(Entity_HasComponent);
		TOAST_ADD_INTERNAL_CALL(Entity_FindEntityByName);

		TOAST_ADD_INTERNAL_CALL(TagComponent_GetTag);
		TOAST_ADD_INTERNAL_CALL(TagComponent_SetTag);

		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_GetRotation);
		TOAST_ADD_INTERNAL_CALL(TransformComponent_SetRotation);
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

		TOAST_ADD_INTERNAL_CALL(MeshComponent_GeneratePlanet);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_PlayAnimation);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_StopAnimation);
		TOAST_ADD_INTERNAL_CALL(MeshComponent_GetDurationAnimation);

		TOAST_ADD_INTERNAL_CALL(CameraComponent_GetFarClip);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_SetFarClip);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_GetNearClip);
		TOAST_ADD_INTERNAL_CALL(CameraComponent_SetNearClip);

		TOAST_ADD_INTERNAL_CALL(PlanetComponent_GetRadius);
		TOAST_ADD_INTERNAL_CALL(PlanetComponent_GetSubdivisions);
		TOAST_ADD_INTERNAL_CALL(PlanetComponent_GetDistanceLUT);
		TOAST_ADD_INTERNAL_CALL(PlanetComponent_GeneratePlanet);

		TOAST_ADD_INTERNAL_CALL(UIButtonComponent_GetColor);
		TOAST_ADD_INTERNAL_CALL(UIButtonComponent_SetColor);

		TOAST_ADD_INTERNAL_CALL(UITextComponent_GetText);
		TOAST_ADD_INTERNAL_CALL(UITextComponent_SetText);
	}

}