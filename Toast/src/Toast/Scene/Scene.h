#pragma once

#include "Toast/Core/UUID.h"
#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/PlanetSystem.h"

#include "Toast/Renderer/EditorCamera.h"
#include "Toast/Renderer/Frustum.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/ParticleSystem.h"

#include <memory>

#pragma warning(push, 0)
#include <entt.hpp>
#pragma warning(pop)

namespace Toast {

	enum class RenderOverlay {
		NONE = 0, 
		POSITIONS = 1,
		NORMALS = 2, 
		ALBEDOMETALLIC = 3, 
		ROUGHNESS = 4, 
		LPASS = 5, 
		ATMOSPHERICSCATTERING = 6, 
		SSAO = 7, 
		SSAOBLUR = 8, 
		BLOOM = 9, 
		BLOOMHALF = 10,
		BLOOMQUARTER = 11,
		BLOOMFINAL = 12, 
		AUTOEXPOSUREGROUP = 13
	};

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
		struct ExposureParams
		{
			float LogLumMin = -16.0f;
			float LogLumMax = 8.0f;
			float RejectBrightNits = 4.0f;
			float RejectBrightSoftNits = 2.0f;
			float RejectDark = 0.002f;
			float CenterWeight = 0.85f;
			float LastEV = 0.0f;
			float KeyValue = 0.25f;
			float SpeedUp = 1.5f;
			float SpeedDown = 2.5f;
			float MinEV = -10.0f;
			float MaxEV = 12.0f;
			float EVOffset = -1.62f;
		};

		struct BloomParams
		{
			bool Enabled = true;
			float AtmosphereIntensity = 0.55f;
			float SpaceIntensity = 0.25f;
			float AtmosphereThreshold = 1.0f;
			float SpaceThreshold = 4.18f;
			float GlarePW;
		};

		//Settings
		struct Settings
		{
			bool IsDirty = false;

			RenderOverlay RenderOverlaySetting = RenderOverlay::NONE;

			enum class Wireframe { NO = 0, YES = 1, ONTOP = 2 };
			Wireframe WireframeRendering = Wireframe::NO;

			bool Grid = true;
			bool CameraFrustum = true;
			bool SunLightFrustum = true;
			bool BackfaceCulling = true;
			bool FrustumCulling = true;
			bool RenderColliders = false;
			bool RenderUI = true;
			bool Shadows = true;
			bool SSAO = false;
			bool SSAODebugging = false;
			float SSAORadius = 0.5f;
			float SSAObias = 0.025f;
			BloomParams Bloom;
			bool DynamicIBL = true;

			int PhysicSlowmotion = 1;
			int PhysicsFPS = 60;
			float physicsElapsedTime = 0.0;
			float SunFrustumOrthoSize = 500.0f;

			float GodRaysExposure = 0.21f;
			float GodRaysDecay = 0.94f;
			float GodRaysDensity = 3.0f;
			float GodRaysWeight = 0.02f;

			ExposureParams AutoExposure;
		};

		struct Stats
		{
			float TimeSteps = 0.0f;
			float FrameTime = 0.0f;
			float FPS = 0.0f;
			uint32_t VerticesCount = 0;
		};

		struct Environment 
		{
			DirectX::XMFLOAT3 NightAmbient = { 0.0f, 0.0f, 0.0f };

			// Sun
			bool SunDiscToggle = false;
			float SunIntensity = 0.0f;
			float SunDiscRadius = 0.0f;
			float SunEdgeSoftness = 0.0f;
			DirectX::XMFLOAT3 SunWhite = { 1.0f, 1.0f, 1.0f };
			float SpaceDiscBrightnessScale = 1.3f;
			DirectX::XMFLOAT3 WarmTint = { 1.0f, 1.0f, 1.0f };
			float AirHaloIntensity = 0.28f;

			float AirHaloStartFrac = 0.15f;
			float AirHaloFalloffPow = 1.10f; 
			float HorizonRefractionDeg = 0.83f;
			float TwilightBlendDeg = 1.5f;

			float HorizonSoftEdgeDeg = 0.4f; 
			float SpaceHaloWidthDeg = 0.8f; 
			float SpaceHaloIntensity = 0.04f; 
			float SpaceHaloCutoffDeg = 6.0f; 

			// Stars
			float StarNits = 600.0f; // brightness of 1 sun-like star in nits
			float DayFadeStartDeg = 2.0f; // start hiding stars above horizon (e.g. +2.0)
			float DayFadeEndDeg = -2.0f; // fully hidden by (e.g. 0.0 or -2.0)
			float TwilightStartDeg = 0.0f; // start appearing (e.g. 0.0)
			float TwilightEndDeg = -6.0f; // fully visible by (e.g. -6.0)
			float SpaceFadeStart = 0.85f; // altitude norm where space visibility starts (0..1), e.g. 0.85
			float SpaceFadeEnd = 0.98f; // fully visible by (0..1), e.g. 0.98
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
		bool OnMouseMoved(MouseMovedEvent& e);

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, const Ref<EditorCamera> editorCamera);
		void OnViewportResize(uint32_t width, uint32_t height);
		const std::tuple<uint32_t, uint32_t> GetViewportSize() const { return std::make_tuple(mViewportWidth, mViewportHeight); }
		void SetViewportPos(DirectX::XMFLOAT2 absoluteViewportPos) { mViewportPosX = absoluteViewportPos.x; mViewportPosY = absoluteViewportPos.y; }
		const std::tuple<uint32_t, uint32_t> GetViewportPos() { return std::make_tuple(mViewportPosX, mViewportPosY); }

		void SetViewportBounds(DirectX::XMFLOAT2 viewportBounds[2]);

		void SetTimeScale(float scale) { mTimeScale = scale; }
		float GetTimeScale() { return mTimeScale; }

		float& GetSkyboxLod() { return mSkyboxLod; }

		SceneCamera* GetMainCamera() { return mMainCamera; }
		void SetMainCamera(SceneCamera* camera) { mMainCamera = camera; }

		int GetFPS() const { return (int)mStats.FPS; }
		float GetFrameTime() const { return mStats.FrameTime; }
		int GetVertices() const { return (int)mStats.VerticesCount; }

		Entity FindEntityByName(std::string_view name);
		Entity FindEntityByUUID(UUID uuid);

		Entity FindChildEntityByName(std::string_view parentName, std::string_view childName);

		void AddChildEntity(Entity entity, Entity parent);

		Entity AddPrefab(std::string& prefabName);
		std::vector<Entity> GetEntitiesWithPrefab(std::string prefabName);

		const EntityMap& GetEntityMap() const { return mEntityIDMap; }
		void CopyTo(Scene* target);

		UUID GetUUID() const { return mSceneID; }

		Ref<Planet> GetPlanet() { return mPlanet; }

		void SetSelectedEntity(entt::entity entity) { mSelectedEntity = entity; }
		entt::entity GetHoveredEntity() { return mHoveredEntity; }
		void SetHoveredEntity(entt::entity entity) { mHoveredEntity = entity; }

		Settings GetSettings() { return mSettings; }
		Environment& GetEnvirontment() { return mEnvironment; }

		Ref<Frustum> GetFrustum() { return mFrustum; }
		void InvalidateFrustum();

		void SetRenderColliders(bool renderColliders) { mSettings.RenderColliders = renderColliders; }
		bool GetRenderColliders() { return mSettings.RenderColliders; }

		entt::registry& GetRegistry() { return mRegistry; }
	public:
		static Ref<Scene> CreateEmpty();
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	private:
		UUID mSceneID;
		entt::entity mSceneEntity;
		entt::registry mRegistry;

		uint32_t mViewportWidth = 0, mViewportHeight = 0;
		uint32_t mViewportPosX = 0, mViewportPosY = 0;

		EntityMap mEntityIDMap;

		Ref<TextureCube> mSkyboxTexture = nullptr;
		float mSkyboxLod = 0.0f;

		LightEnvironment mLightEnvironment;

		Environment mEnvironment;
		Settings mSettings;
		Stats mStats;

		Ref<Material> mCubeColliderMaterial, mSphereColliderMaterial;

		bool mIsRunning = false;
		bool mIsPaused = false;

		float mTimeScale = 1.0f;

		entt::entity mSelectedEntity;
		entt::entity mHoveredEntity;

		Ref<Planet> mPlanet;

		SceneCamera* mMainCamera = nullptr;

		Ref<Frustum> mFrustum;
		bool mInvalidatePlanet = false;

		DirectX::XMFLOAT2 mViewportBounds[2];

		Ref<ParticleSystem> mParticleSystem;

		friend class Entity;
		friend class Renderer;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class SceneSettingsPanel;
		friend class Prefab;
	};
}
	