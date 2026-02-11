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

	class PhysicsEngine;

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
		SKYVIEWLUT = 13
	};

	struct DirectionalLight
	{
		DirectX::XMMATRIX ViewProjectionMatrix = DirectX::XMMatrixIdentity();
		DirectX::XMFLOAT4 Direction = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 Radiance = { 0.0f, 0.0f, 0.0f, 0.0f };

		float Multiplier = 1.0f;
		float SunDisc = 0.0f;
	};

	struct SunUVResult
	{
		DirectX::XMFLOAT2 uv;     // [0..1] ideally (may go outside if off-screen)
		bool valid;  // false if sun is behind camera / cannot project
	};

	inline SunUVResult ComputeSunUVFromDirection(const DirectX::XMFLOAT3& directionFromLight, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
	{
		SunUVResult out{};
		out.uv = DirectX::XMFLOAT2(0.5f, 0.5f);
		out.valid = false;

		// TO sun (matches your shader: wSun = -normalize(direction.xyz))
		DirectX::XMVECTOR wSun = DirectX::XMVectorSet(-directionFromLight.x,
			-directionFromLight.y,
			-directionFromLight.z,
			0.0f);
		wSun = DirectX::XMVector3Normalize(wSun);

		// Transform direction to view space (w=0 for direction)
		DirectX::XMVECTOR sunVS = DirectX::XMVector3TransformNormal(wSun, view);

		// Extract components
		DirectX::XMFLOAT3 sVS;
		DirectX::XMStoreFloat3(&sVS, sunVS);

		// In a conventional left-handed D3D view space, camera looks down +Z.
		// The sun is "in front" if z > 0.
		const float z = sVS.z;
		if (z <= 1e-6f)
		{
			// Behind camera (or too close to perpendicular to project safely)
			return out;
		}

		// We only need P00 and P11 from projection matrix.
		// In DirectXMath, XMMATRIX is row-major in memory; indices m.r[row].m128_f32[col].
		const float P00 = proj.r[0].m128_f32[0];
		const float P11 = proj.r[1].m128_f32[1];

		// NDC from direction (equivalent to projecting a point on the ray)
		const float ndcX = (sVS.x / z) * P00;
		const float ndcY = (sVS.y / z) * P11;

		// NDC -> UV. Your shaders use ndcY = 1 - 2*uv.y, so invert here.
		const float u = 0.5f * ndcX + 0.5f;
		const float v = 0.5f - 0.5f * ndcY;

		out.uv = DirectX::XMFLOAT2(u, v);
		out.valid = true;
		return out;
	}

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
			float EVSurfaceDay = 0.0f;
			float EVSpaceDay = 0.0f;
			float EVSurfaceNight = 0.0f;
			float EVSpaceNight = 0.0f;
			DirectX::XMFLOAT2 AltFadeFrac = { 0.0f, 0.0f };
			DirectX::XMFLOAT2 SunFadeDeg = { 0.0f, 0.0f };
		};

		struct BloomParams
		{
			bool Enabled = true;
			float SunSurfaceThreshold = 0.0f;
			float SunSurfaceIntensity = 0.0f;
			float SunSpaceThreshold = 0.0f;
			float SunSpaceIntensity = 0.0f;
			float SkySurfaceThreshold = 0.0f;
			float SkySurfaceIntensity = 0.0f;
			float SkySpaceThreshold = 0.0f;
			float SkySpaceIntensity = 0.0f;
			float GeometryThreshold = 0.0f;
			float GeometryIntensity = 0.0f;

			float SunRadius = 0.0f;            // kernel size scale for SUN layer
			float SkySurfaceRadius = 0.0f;     // kernel size scale for sky near ground (wider haze)
			float SkySpaceRadius = 0.0f;      // kernel size scale for sky in space (tighter limb)
			float SoftKnee = 0.0f;           // 0..1 knee around threshold
			float SaturationClamp = 0.0f;      // 0..1 desat of bloom to prevent color smear (e.g. 0.85)
		};

		struct GodRayParams 
		{
			float Exposure = 0.21f;
			float Decay = 0.94f;
			float Density = 3.0f;
			float Weight = 0.02f;
			float KHalo = 0.6f;
			float HaloPower = 2.0f;
			float FogRangeMeters = 120000.0f;
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
			float FrustumCullingMargin = 1.0f; // multiplier on camera near/far planes for culling (e.g. 1.1 to be slightly more lenient)
			bool SunLightFrustum = true;
			bool RenderColliders = false;
			bool RenderUI = true;
			bool Shadows = true;
			bool SSAO = false;
			bool SSAODebugging = false;
			float DirectionalLightningGain = 1.0f;
			float SSAORadius = 0.5f;
			float SSAObias = 0.025f;
			BloomParams Bloom;
			bool DynamicIBL = true;

			float SunFrustumOrthoSize = 500.0f;

			GodRayParams GodRays;

			ExposureParams Exposure;
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

			SunUVResult SunUV;

			// Lightning Gains
			float DiffuseIBLGain = 1.0f;
			float SpecularIBLGain = 1.0f;

			// Sun
			bool SunDiscToggle = false;
			float SunIntensity = 20.0f;
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

			float SpaceHaloWidthDeg = 0.8f; 
			float SpaceHaloIntensity = 0.04f; 
			float SpaceHaloCutoffDeg = 6.0f; 

			int SunSpikes = 6; // number of diffraction spikes (4,6,8)
			float SunSpikeSharpness = 24.0f; // higher=thinner (e.g. 24.0)
			float SunSpikeRadiusSurface = 0.1f;
			float SunSpikeRadiusSpace = 0.2f;

			float SunSpikeFallOff = 2.0f; // how quickly spikes fade with altitude
			float SunSpikeStrengthSurface = 0.0f;
			float SunSpikeStrengthSpace = 0.08f;
			float SunGlareStrengthSurface = 0.005f;

			float SunGlareStrengthSpace = 0.015f;
			float SunGlareRadiusSurface = 0.10f;
			float SunGlareRadiusSpace = 0.06f;
			float LensAltStart = 0.60f; // altitude norm where lens effects start (0..1), e.g. 0.5

			float LensAltEnd = 1.0f; // fully on by (0..1), e.g. 0.8
			float GhostStrength = 0.10f;
			float GhostSpacing = 1.0f;
			float GhostFalloff = 0.85f;

			float GhostSizeSurface = 0.0012f;
			float GhostSizeSpace = 0.00045f;
			float GhostAirSuppression = 0.1f;

			// Stars
			float StarNits = 600.0f; // brightness of 1 sun-like star in nits
			float TwilightStartDeg = 0.0f; // start appearing (e.g. 0.0)
			float TwilightEndDeg = -6.0f; // fully visible by (e.g. -6.0)
			float SpaceFadeStart = 0.85f; // altitude norm where space visibility starts (0..1), e.g. 0.85
			float SpaceFadeEnd = 0.98f; // fully visible by (0..1), e.g. 0.98
		};

		Scene(std::string name = "Default Scene");
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

		Entity FindParentEntity(UUID childID);
		Entity FindChildEntityByName(std::string_view parentName, std::string_view childName);
		Entity FindDescendantByName(Entity parent, std::string_view nameStr);

		void AddChildEntity(Entity entity, Entity parent);
		void AddMeshPartEntities(std::unordered_map<std::string, UUID>& parts, Entity& meshParent);

		uint32_t GetNextPrefabIndex(const std::string& prefabName);
		Entity AddPrefab(std::string& prefabName);
		std::vector<Entity> GetEntitiesWithPrefab(std::string prefabName);

		const EntityMap& GetEntityMap() const { return mEntityIDMap; }
		void CopyTo(Scene* target);

		UUID GetUUID() const { return mSceneID; }
		void SetName(const std::string& name) { mName = name; }
		const std::string& GetName() const { return mName; }

		Ref<Planet> GetPlanet() { return mPlanet; }

		void SetActiveCamera(Ref<Camera> camera) { mActiveCamera = camera; }
		Ref<Camera> GetActiveCamera() { return mActiveCamera; }

		void SetSelectedEntity(entt::entity entity) { mSelectedEntity = entity; }
		entt::entity GetHoveredEntity() { return mHoveredEntity; }
		void SetHoveredEntity(entt::entity entity) { mHoveredEntity = entity; }

		Settings& GetSettings() { return mSettings; }
		Environment& GetEnvirontment() { return mEnvironment; }

		Ref<Frustum> GetFrustum() { return mFrustum; }
		void InvalidateFrustum();

		void SetRenderColliders(bool renderColliders) { mSettings.RenderColliders = renderColliders; }
		bool GetRenderColliders() { return mSettings.RenderColliders; }

		float GetAltitude(Entity& entity, bool ignoreWorldTranslation = false);
		float GetAltitudeAtWorldPos(const Vector3& worldPos, double& outRadialDist, Vector3& outGroundNormal);

		Ref<PhysicsEngine> GetPhysicsEngine() { return mPhysicsEngine; }

		entt::registry& GetRegistry() { return mRegistry; }

		void SetRuntimeBlocked(bool blocked) { mRuntimeBlocked = blocked; }
		bool IsRuntimeBlocked() const { return mRuntimeBlocked; }
	public:
		static Ref<Scene> CreateEmpty();
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	private:
		UUID mSceneID;
		std::string mName;

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

		entt::entity mSelectedEntity = entt::null;
		entt::entity mHoveredEntity = entt::null;

		Ref<Planet> mPlanet;

		SceneCamera* mMainCamera = nullptr;

		Ref<Camera> mActiveCamera = nullptr;

		Ref<Frustum> mFrustum;
		bool mInvalidatePlanet = false;

		DirectX::XMFLOAT2 mViewportBounds[2];

		Ref<ParticleSystem> mParticleSystem;

		Ref<PhysicsEngine> mPhysicsEngine;

		bool mRuntimeBlocked = false;

		friend class Entity;
		friend class Renderer;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class SceneSettingsPanel;
		friend class Prefab;
		friend class PhysicsEngine;
	};
}
	