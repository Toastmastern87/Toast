#include "tpch.h"
#include "ScriptGlue.h"
#include "ScriptEngine.h"

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"

#include "mono/metadata/object.h"

namespace Toast {

#define TOAST_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Toast.InternalCalls::" #Name, Name)

	static void NativeLog_Vector(DirectX::XMFLOAT3* parameter, DirectX::XMFLOAT3* outResult)
	{
		TOAST_CORE_WARN("Vector3: %f, %f, %f", parameter->x, parameter->y, parameter->z);

		DirectX::XMStoreFloat3(outResult, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(parameter)));
	}

	static void NativeLog(MonoString* string, int parameter)
	{
		char* cStr = mono_string_to_utf8(string);
		std::string str(cStr);

		mono_free(cStr);

		std::cout << str << ", " << parameter << std::endl;
	}

	static void Entity_GetTransform(UUID entityID, DirectX::XMMATRIX* outTransform)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		Entity entity = scene->FindEntityByUUID(entityID);
		*outTransform = entity.GetComponent<TransformComponent>().Transform;
	}

	static void Entity_SetTransform(UUID entityID, DirectX::XMMATRIX* transform)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		Entity entity = scene->FindEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().Transform = *transform;
	}

	static bool Input_IsKeyPressed(KeyCode key)
	{
		return Input::IsKeyPressed(key);
	}

	void ScriptGlue::RegisterFunctions()
	{
		TOAST_ADD_INTERNAL_CALL(NativeLog);
		TOAST_ADD_INTERNAL_CALL(NativeLog_Vector);

		TOAST_ADD_INTERNAL_CALL(Entity_GetTransform);
		TOAST_ADD_INTERNAL_CALL(Entity_SetTransform);

		TOAST_ADD_INTERNAL_CALL(Input_IsKeyPressed);
	}

}