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
		DirectX::XMMATRIX ViewProjectionMatrix = DirectX::XMMatrixIdentity();
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
			bool IsDirty = false;

			enum class PlanetOverlay { NONE = 0, ALBEDO = 1, HEIGHTMAP = 2, NORMAL = 3, COLOR = 4 };
			PlanetOverlay PlanetOverlaySetting = PlanetOverlay::NONE;

			enum class Wireframe { NO = 0, YES = 1, ONTOP = 2 };
			Wireframe WireframeRendering = Wireframe::NO;

			bool Grid = true;
			bool CameraFrustum = true;
			bool SunLightFrustum = true;
			bool BackfaceCulling = true;
			bool FrustumCulling = true;
			bool RenderColliders = false;
			bool RenderUI = true;

			int PhysicSlowmotion = 1;
			int PhysicsFPS = 60;
			float physicsElapsedTime = 0.0;
			float SunFrustumOrthoSize = 500.0f;
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

		Entity CreateEntity(const std::string& name = std::string(), UUID parent = 0);
		Entity CreateEntityWithID(UUID uuid, const std::string& name);
		void DestroyEntity(Entity entity);

		void OnRuntimeStart();
		void OnRuntimeStop();

		bool IsRunning() const { return mIsRunning; }
		bool IsPaused() const { return mIsPaused; }
		void SetPaused(bool paused) { mIsPaused = paused; }

		void OnEvent(Event& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, const Ref<EditorCamera> editorCamera);
		void OnViewportResize(uint32_t width, uint32_t height);
		const float GetViewportWidth() const { return mViewportWidth; }

		void SetTimeScale(float scale) { mTimeScale = scale; }
		float GetTimeScale() { return mTimeScale; }

		float& GetSkyboxLod() { return mSkyboxLod; }
		float& GetEnvironmentIntensity() { return mEnvironmentIntensity; }

		const Environment& GetEnvironment() const { return mEnvironment; }

		int GetFPS() const { return (int)mStats.FPS; }
		float GetFrameTime() const { return mStats.FrameTime; }
		int GetVertices() const { return (int)mStats.VerticesCount; }

		Entity FindEntityByName(std::string_view name);
		Entity FindEntityByUUID(UUID uuid);

		Entity FindChildEntityByName(std::string_view parentName, std::string_view childName);

		void AddChildEntity(Entity entity, Entity parent);

		const EntityMap& GetEntityMap() const { return mEntityIDMap; }
		void CopyTo(Ref<Scene>& target);

		UUID GetUUID() const { return mSceneID; }

		void SetSelectedEntity(entt::entity entity) { mSelectedEntity = entity; }
		void SetHoveredEntity(entt::entity entity) { mHoveredEntity = entity; }

		Settings GetSettings() { return mSettings; }

		Ref<Frustum> GetFrustum() { return mFrustum; }
		void InvalidateFrustum();

		void SetRenderColliders(bool renderColliders) { mSettings.RenderColliders = renderColliders; }
		bool GetRenderColliders() { return mSettings.RenderColliders; }
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
		Ref<TextureCube> mSkyboxTexture = nullptr;
		float mEnvironmentIntensity = 1.0f, mSkyboxLod = 0.0f;

		LightEnvironment mLightEnvironment;

		Settings mSettings;
		Stats mStats;

		Ref<Material> mCubeColliderMaterial, mSphereColliderMaterial;

		bool mIsRunning = false;
		bool mIsPaused = false;

		float mTimeScale = 1.0f;

		entt::entity mSelectedEntity;
		entt::entity mHoveredEntity;

		Ref<Frustum> mFrustum;
		bool mInvalidatePlanet = false;

		friend class Entity;
		friend class Renderer;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class SceneSettingsPanel;
	};
}
	