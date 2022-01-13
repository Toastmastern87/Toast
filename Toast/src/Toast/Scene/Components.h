#pragma once

#include <DirectXMath.h>

#include "Toast/Core/UUID.h"

#include "Toast/Scene/SceneCamera.h"

#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/PlanetSystem.h"
#include "Toast/Renderer/SceneEnvironment.h"

namespace Toast {

	struct IDComponent
	{
		UUID ID = 0;
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {}
	};

	struct TransformComponent
	{
		DirectX::XMMATRIX Transform;
		DirectX::XMFLOAT3 RotationEulerAngles;

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const DirectX::XMMATRIX& transform)
			: Transform(Transform) { RotationEulerAngles = { 0.0f, 0.0f, 0.0f }; }

		operator DirectX::XMMATRIX& () { return Transform; }
		operator const DirectX::XMMATRIX& () const { return Transform; }
	};

	struct MeshComponent
	{
		Ref<Toast::Mesh> Mesh;

		MeshComponent() = default;
		MeshComponent(const MeshComponent& other) = default;
		MeshComponent(const Ref<Toast::Mesh>& mesh)
			: Mesh(mesh) {}

		operator Ref<Toast::Mesh>() { return Mesh; }
	};

	struct PlanetComponent
	{
		struct PlanetGPUData
		{
			DirectX::XMFLOAT4 radius = { 3389.5f, 3389.5f, 3389.5f, 3389.5f };
			DirectX::XMFLOAT4 minAltitude = { -8.2f, -8.2f, -8.2f, -8.2f };
			DirectX::XMFLOAT4 maxAltitude = { 21.2f, 21.2f, 21.2f, 21.2f };
		};

		Ref<Toast::Mesh> Mesh;
		std::vector<float> DistanceLUT;
		std::vector<float> FaceLevelDotLUT;
		std::vector<float> HeightMultLUT;
		int16_t Subdivisions = 0;
		int16_t PatchLevels = 1;
		bool Atmosphere = false;

		PlanetGPUData PlanetData;

		PlanetComponent() = default;
		PlanetComponent(int16_t subdivisions, int16_t patchLevels, DirectX::XMFLOAT4 maxAltitude, DirectX::XMFLOAT4 minAltitude, DirectX::XMFLOAT4 radius)
			: Subdivisions(subdivisions), PatchLevels(patchLevels)
		{
			PlanetData.maxAltitude = maxAltitude;
			PlanetData.minAltitude = minAltitude;
			PlanetData.radius = radius;
		}
		PlanetComponent(const PlanetComponent& other) = default;
	};

	struct SpriteRendererComponent
	{
		DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const DirectX::XMFLOAT4& color)
			: Color(color) {}
	};

	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct ScriptComponent
	{
		std::string ModuleName;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
		ScriptComponent(const std::string& moduleName)
			: ModuleName(moduleName) {}
	};

	struct DirectionalLightComponent 
	{
		DirectX::XMFLOAT3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		bool SunDisc = false;
	};

	struct SkyLightComponent 
	{
		Environment SceneEnvironment;
		float Intensity = 1.0f;
	};
}