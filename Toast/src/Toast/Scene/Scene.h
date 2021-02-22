#pragma once

#include "entt.hpp"
#include "Toast/Core/Timestep.h"
#include "Toast/Renderer/PerspectiveCamera.h"
#include "Toast/Renderer/SceneEnvironment.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/Mesh.h"

namespace Toast {

	struct DirectionalLight
	{
		DirectX::XMFLOAT4 Direction = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 Radiance = { 0.0f, 0.0f, 0.0f, 0.0f };

		float Multiplier = 1.0f;
		float SunDisc = 0.0f;
	};

	struct LightEnvironment
	{
		DirectionalLight DirectionalLights[1];
	};


	class Entity;

	class Scene 
	{
	public:
		Scene();
		~Scene();

		void Init();

		Entity CreateEntity(const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		Entity CreateCube(const std::string& name = std::string());
		Entity CreateSphere(const std::string& name = std::string());

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, const Ref<PerspectiveCamera> perspectiveCamera);
		void OnViewportResize(uint32_t width, uint32_t height);

		float& GetSkyboxLod() { return mSkyboxLod; }
		float& GetEnvironmentIntensity() { return mEnvironmentIntensity; }

		const Environment& GetEnvironment() const { return mEnvironment; }
		void SetSkybox(const Ref<TextureCube>& skybox);

		//Settings
		struct Settings
		{
			enum class Wireframe { NO = 0, YES = 1, ONTOP = 2 };
			Wireframe WireframeRendering = Wireframe::NO;
		};
		struct Stats
		{
			float timesteps = 0.0f;
			float FPS;
			uint32_t VerticesCount = 0;
		};
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	private:
		entt::registry mRegistry;
		uint32_t mViewportWidth = 0, mViewportHeight = 0;

		Environment mEnvironment;
		Ref<Material> mSkyboxMaterial;
		Ref<TextureCube> mSkyboxTexture = nullptr;
		float mEnvironmentIntensity = 1.0f, mSkyboxLod = 0.0f;
		Ref<Mesh> mSkybox;

		LightEnvironment mLightEnvironment;

		Settings mSettings;
		Stats mStats;

		DirectX::XMVECTOR mOldCameraPos = { 0.0f, 0.0f, 0.0f }, mOldCameraRot = { 0.0f, 0.0f, 0.0f }, mOldCameraScale = { 0.0f, 0.0f, 0.0f };

		friend class Entity;
		friend class Renderer;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class SceneSettingsPanel;
	};
}
	