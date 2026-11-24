#pragma once

#include "Toast/Core/UUID.h"
#include "Toast/Core/Timestep.h"

#include "Toast/Events/KeyEvent.h"
#include "Toast/Events/MouseEvent.h"

#include "Toast/Renderer/RendererConstants.h"
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

	// Forward Declarations
	class PhysicsEngine;
	class SelectionSystem;
	class MovementSystem;
	class AudioSystem;

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
		BLOOMFINAL = 10,
		SKYVIEWLUT = 11,
		PLANETMATERIALS = 12,
	};

	struct DirectionalLight
	{
		DirectX::XMMATRIX LightViewProj[MaxCascades];
		DirectX::XMFLOAT4 Direction = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 Radiance = { 0.0f, 0.0f, 0.0f, 0.0f };

		float Multiplier = 1.0f;
		float CascadeEnds[MaxCascades];
		uint32_t CascadeCount;
		float ShadowDistance;
		float ConstantBias;
		float SlopeBias;
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
			bool LockEV = false;
			float LockedEV = 0.0f;

			float Compensation = -0.5f;       // stops; + brighter, − darker
			float MinEV = -6.0f;            // limit for dark scenes (higher = darker dusk/night)
			float MaxEV = 6.0f;              // limit for bright scenes
			float OrbitMinEV = -6.0f;			// Min EV above the atmosphere (blended in by the camera's space factor); higher = darker night side from orbit
			float AdaptationKneeEV = -1.8f; // Down to this EV the exposure fully follows the light (daylight)
			float DimLightAdaptation = 0.4f; // Below the knee, the fraction of the drop it follows (dusk stays dusk)
			float LocalHighlightContrast = 0.7f; // < 1 compresses bright areas towards middle grey (sun glow, sunset sky)
			float LocalShadowContrast = 1.0f; // < 1 lifts dark areas towards middle grey
			float LocalDetailStrength = 1.0f; // 1 = keep the pixels' detail relative to their area
			float SpeedToBright = 0.5f;      // 1/s, camera opening up (scene got darker)
			float SpeedToDark = 3.0f;        // 1/s, camera stopping down (scene got brighter); your Slice 2 value
		};

		struct CascadedShadowMapParams
		{
			bool Active = true;
			bool IsDirty = true;
			uint32_t CascadeCount = 4;        // runtime (<= MaxCascades)
			float ShadowDistance = 5000.0f;   // in your world units
			float Lambda = 0.6f;              // 0 = uniform, 1 = logarithmic
			float ConstantBias = 0.0005;
			float SlopeScaledBias = 0.0025;
		};

		struct BloomParams
		{
			bool Enabled = true;
			float Intensity = 0.04f;
			float Falloff = 0.5f;
		};

		struct OutlineSettings 
		{
			DirectX::XMFLOAT4 Color = { 1.0f, 0.6f, 0.1f, 1.0f }; // amber default
			float Thickness = 2.0f;  // pixels
			float Softness = 1.0f;  // edge feathering
			float PulseSpeed = 0.0f;  // 0 = static
			bool XRay = false; // visible through occluders
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
			bool ShowUIBounds = false;
			float FrustumCullingMargin = 1.0f; // multiplier on camera near/far planes for culling (e.g. 1.1 to be slightly more lenient)
			bool RenderColliders = false;
			bool RenderUI = true;
			bool SSAO = false;
			bool SSAODebugging = false;
			float DirectionalLightningGain = 1.0f;
			float SSAORadius = 0.5f;
			float SSAObias = 0.025f;
			BloomParams Bloom;

			float SunFrustumOrthoSize = 500.0f;

			DirectX::XMFLOAT4 HoverTintColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // Hover Tint

			ExposureParams Exposure;

			CascadedShadowMapParams Shadows;

			OutlineSettings Outline;
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
			// Lightning Gains
			float DiffuseIBLGain = 1.0f;
			float SpecularIBLGain = 1.0f;
			float NightSkyLight = 0.0015f; // uniform night-sky radiance (starlight), E/pi units

			// Sun
			bool SunDiscToggle = false;
			float SunIntensity = 20.0f;
			float SunDiscRadius = 0.0f;	

			AssetHandle StarburstTextureHandle = 0;

			float StarburstSize = 6; // sprite diameter as a fraction of the screen height; the rays end at its edge
			float StarburstIntensity = 6000.0f; // brightness, before the sun's colour and visibility
			float GhostIntensity = 1.0f;
			float GhostScale = 1.0f;

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

		void StopAllSounds();

		bool IsRunning() const { return mIsRunning; }
		bool IsPaused() const { return mIsPaused; }
		void SetPaused(bool paused) { mIsPaused = paused; }

		void OnEvent(Event& e);
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnKeyReleased(KeyReleasedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);
		bool OnMouseMoved(MouseMovedEvent& e);
		bool OnMouseScrolled(MouseScrolledEvent& e);

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
		void AddMeshPartEntities(MeshComponent& mc, Entity owner);

		DirectX::XMMATRIX ComposeWorldTransform(Entity entity);
		void UnparentEntity(Entity entity);                   // world-pose-preserving detach

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

		const OutlineSettings& GetOutlineSettings() const { return mSettings.Outline; }
		OutlineSettings& GetOutlineSettings() { return mSettings.Outline; }

		SelectionSystem& GetSelectionSystem() { return *mSelectionSystem; }
		MovementSystem& GetMovementSystem() { return *mMovementSystem; }

		bool GetWorldPositionUnderCursor(Vector3& outWorldPos);

		// Animation functions
		void ResetMeshAnimations(MeshComponent& mc);
		bool IsAnimationComplete(const MeshComponent& mc, const std::string& name);

		// Scripting helpers
		void ResolveScriptClassNames(const std::string& projectNamespace);

		DirectX::XMFLOAT2 GetViewportMousePosition() { return { mMouseX - mViewportBounds[0].x, mMouseY - mViewportBounds[0].y }; }

		static Ref<Scene> CreateEmpty();
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
		void UpdateHoveredEntity();
		void UpdatePickedWorldPosition();

		OutlineSettings ResolveOutlineSettings(Entity selected);

		bool ProjectConnectorAnchor(DirectX::XMVECTOR worldPos, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, bool clampToEdge, float margin, DirectX::XMFLOAT2& outScreenPos);

		void UpdateUIButtons(float ts);
		void UpdateUIPanelDrag();
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

		Ref<PhysicsEngine> mPhysicsEngine;

		Scope<SelectionSystem> mSelectionSystem;
		Scope<MovementSystem> mMovementSystem;
		Scope<AudioSystem> mAudioSystem;

		bool mRuntimeBlocked = false;

		// Mouse Picking
		float mMouseX = 0.0f;
		float mMouseY = 0.0f;
		bool mPickingReadbackPending = false;
		bool mPositionReadbackPending = false;
		DirectX::XMFLOAT3 mLastPickedWorldPos;
		bool mLastPickedValid = false;

		entt::entity mUIPressedEntity = entt::null;
		entt::entity mUIReleasedEntity = entt::null;
		bool mUIReleaseOccurred = false;

		entt::entity mUIDraggedEntity = entt::null;
		DirectX::XMFLOAT2 mUIDragLastCursor = { 0.0f, 0.0f };

		friend class Entity;
		friend class Renderer;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class SceneSettingsPanel;
		friend class Prefab;
		friend class PhysicsEngine;
		friend class SelectionSystem;
		friend class MovementSystem;
	};
}
	