#pragma once

#include "Toast/Core/Input.h"

#include "Toast/Script/ScriptEngine.h"

#include <vector>

extern "C" {
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
}

namespace Toast {

	namespace Script {

		// Console
		void Toast_Console_LogTrace(MonoObject* message);
		void Toast_Console_LogInfo(MonoObject* message);
		void Toast_Console_LogWarning(MonoObject* message);
		void Toast_Console_LogError(MonoObject* message);
		void Toast_Console_LogCritical(MonoObject* message);

		// Input
		bool Toast_Input_IsKeyPressed(KeyCode key);
		bool Toast_Input_IsMouseButtonPressed(MouseCode button);
		void Toast_Input_GetMousePosition(DirectX::XMFLOAT2* outPos);
		float Toast_Input_GetMouseWheelDelta();
		void Toast_Input_SetMouseWheelDelta(float value);

		// Entity
		void Toast_Entity_CreateComponent(uint64_t entityID, void* type);
		bool Toast_Entity_HasComponent(uint64_t entityID, void* type);
		uint64_t Toast_Entity_FindEntityByTag(MonoString* tag);

		// Tag Component
		MonoString* Toast_TagComponent_GetTag(uint64_t entityID);
		void Toast_TagComponent_SetTag(uint64_t entityID, MonoString* inTag);

		// Transform Component
		void Toast_TransformComponent_GetTransform(uint64_t entityID, DirectX::XMMATRIX* outTransform);
		void Toast_TransformComponent_SetTransform(uint64_t entityID, DirectX::XMMATRIX* inTransform);
		void Toast_TransformComponent_GetTranslation(uint64_t entityID, DirectX::XMFLOAT3* outTranslation);
		void Toast_TransformComponent_SetTranslation(uint64_t entityID, DirectX::XMFLOAT3* inTranslation);
		void Toast_TransformComponent_GetRotation(uint64_t entityID, DirectX::XMFLOAT3* outRotation);
		void Toast_TransformComponent_SetRotation(uint64_t entityID, DirectX::XMFLOAT3* inRotation);
		void Toast_TransformComponent_GetScale(uint64_t entityID, DirectX::XMFLOAT3* outScale);
		void Toast_TransformComponent_SetScale(uint64_t entityID, DirectX::XMFLOAT3* inScale);

		// Planet Component
		void Toast_PlanetComponent_GetRadius(uint64_t entityID, float* outRadius);
		void Toast_PlanetComponent_GetSubdivisions(uint64_t entityID, int* outRadius);
		MonoArray* Toast_PlanetComponent_GetDistanceLUT(uint64_t entityID);
		void Toast_PlanetComponent_GeneratePlanet(uint64_t entityID, DirectX::XMFLOAT3* cameraPos, DirectX::XMMATRIX* cameraTransform);

		// Mesh Component
		void Toast_MeshComponent_GeneratePlanet(uint64_t entityID, DirectX::XMFLOAT3* cameraPos, DirectX::XMMATRIX* cameraTransform);

		// Camera Component
		float Toast_CameraComponent_GetFarClip(uint64_t entityID);
		void Toast_CameraComponent_SetFarClip(uint64_t entityID, float inFarClip);
		float Toast_CameraComponent_GetNearClip(uint64_t entityID);
		void Toast_CameraComponent_SetNearClip(uint64_t entityID, float inNearClip);
	}

}