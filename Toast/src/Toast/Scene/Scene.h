#pragma once

#include "Toast/Core/UUID.h"
#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/EditorCamera.h"
#include "Toast/Renderer/Frustum.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/SceneEnvironment.h"

#include <memory>

#pragma warning(push, 0)
#include <entt.hpp>
#pragma warning(pop)

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
	using EntityMap = std::unordered_map<UUID, Entity>;

	class Scene : public std::enable_shared_from_this<Scene>
	{
	public:
		//Settings
		struct Settings
		{
			enum class Wireframe { NO = 0, YES = 1, ONTOP = 2 };
			Wireframe WireframeRendering = Wireframe::NO;

			bool Grid = true;
			bool CameraFrustum = true;
			bool BackfaceCulling = true;
			bool FrustumCulling = true;
		};
		struct Stats
		{
			float TimeSteps = 0.0f;
			float FrameTime = 0.0f;
			float FPS = 0.0f;
			uint32_t VerticesCount = 0;
		};

		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithID(UUID uuid, const std::string& name);
		void DestroyEntity(Entity entity);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, const Ref<EditorCamera> editorCamera);
		void OnViewportResize(uint32_t width, uint32_t height);

		float& GetSkyboxLod() { return mSkyboxLod; }
		float& GetEnvironmentIntensity() { return mEnvironmentIntensity; }

		const Environment& GetEnvironment() const { return mEnvironment; }
		void SetSkybox(Ref<TextureCube> skybox);

		int GetFPS() const { return (int)mStats.FPS; }
		float GetFrameTime() const { return mStats.FrameTime; }
		int GetVertices() const { return (int)mStats.VerticesCount; }

		Entity FindEntityByTag(const std::string& tag);

		const EntityMap& GetEntityMap() const { return mEntityIDMap; }
		void CopyTo(Ref<Scene>& target);

		UUID GetUUID() const { return mSceneID; }

		void SetSelectedEntity(entt::entity entity) { mSelectedEntity = entity; }

		Settings GetSettings() { return mSettings; }

		Frustum* GetFrustum() { return mFrustum.get(); }
		void InvalidateFrustum();

		void SetOldCameraTransform(DirectX::XMMATRIX transform) { mOldCameraTransform = transform; }
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	private:
		UUID mSceneID;
		entt::entity mSceneEntity;
		entt::registry mRegistry;

		uint32_t mViewportWidth = 0, mViewportHeight = 0;

		EntityMap mEntityIDMap;

		Environment mEnvironment;
		Ref<Material> mSkyboxMaterial;
		Ref<TextureCube> mSkyboxTexture = nullptr;
		float mEnvironmentIntensity = 1.0f, mSkyboxLod = 0.0f;
		Ref<Mesh> mSkybox;

		LightEnvironment mLightEnvironment;

		Settings mSettings;
		Stats mStats;

		DirectX::XMVECTOR mOldCameraPos = { 0.0f, 0.0f, 0.0f }, mOldCameraRot = { 0.0f, 0.0f, 0.0f }, mOldCameraScale = { 0.0f, 0.0f, 0.0f };
		DirectX::XMMATRIX mOldCameraTransform = DirectX::XMMatrixIdentity();

		bool mIsPlaying = false;

		entt::entity mSelectedEntity;

		bool mOldBackfaceCullSetting = false;
		bool mOldFrustumCullSetting = false;

		Ref<Frustum> mFrustum;

		friend class Entity;
		friend class Renderer;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class SceneSettingsPanel;

		friend void OnScriptComponentConstruct(entt::registry& registry, entt::entity entity);
		friend void OnScriptComponentDestroy(entt::registry& registry, entt::entity entity);
	};
}
	