#pragma once

#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/Formats.h"

#include "Toast/Core/Math/Math.h"

#include "Toast/Core/UUID.h"

#include <DirectXMath.h>
#include <unordered_set>

#pragma push_macro("free")
#pragma push_macro("malloc")
#undef free
#undef malloc

#include <../cgltf/include/cgltf.h>

#pragma pop_macro("malloc")
#pragma pop_macro("free")

namespace Toast {

	struct Face
	{
		std::tuple<uint32_t, uint32_t, uint32_t> Indices;

		Face() = default;
	};

	struct Vertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT4 Tangent;
		DirectX::XMFLOAT2 Texcoord;
		DirectX::XMFLOAT3 Color;

		Vertex(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 nor, DirectX::XMFLOAT4 tan, DirectX::XMFLOAT2 uv, DirectX::XMFLOAT3 color)
		{
			Position = pos;
			Normal = nor;
			Tangent = tan;
			Texcoord = uv;
			Color = color;
		}

		Vertex(DirectX::XMFLOAT3 pos)
		{
			Position = pos;
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
			Color = { 0.0f, 0.0f, 0.0f };
		}

		Vertex(Vector3 pos)
		{
			Position = { (float)pos.x, (float)pos.y, (float)pos.z };
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
			Color = { 0.0f, 0.0f, 0.0f };
		}

		Vertex(Vector3 pos, Vector3 color)
		{
			Position = { (float)pos.x, (float)pos.y, (float)pos.z };
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
			Color = { (float)color.x, (float)color.y, (float)color.z };
		}

		Vertex(Vector3 pos, Vector2 uv)
		{
			Position = { (float)pos.x, (float)pos.y, (float)pos.z };
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { (float)uv.x, (float)uv.y };
			Color = { 0.0f, 0.0f, 0.0f };
		}

		Vertex(Vector3 pos, Vector2 uv, Vector3 normal)
		{
			Position = { (float)pos.x, (float)pos.y, (float)pos.z };
			Normal = { (float)normal.x, (float)normal.y, (float)normal.z };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { (float)uv.x, (float)uv.y };
			Color = { 0.0f, 0.0f, 0.0f };
		}

		Vertex()
		{
			Position = { 0.0f, 0.0f, 0.0f };
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
			Color = { 0.0f, 0.0f, 0.0f };
		}

		Vertex operator+(const Vertex& a)
		{
			DirectX::XMFLOAT3 newPos = { this->Position.x + a.Position.x, this->Position.y + a.Position.y, this->Position.z + a.Position.z };
			return Vertex(newPos);
		}

		Vertex operator*(const float factor)
		{
			DirectX::XMFLOAT3 newPos = { this->Position.x * factor, this->Position.y * factor, this->Position.z * factor };
			return Vertex(newPos);
		}

		Vertex operator/(const float factor)
		{
			DirectX::XMFLOAT3 newPos = { this->Position.x / factor, this->Position.y / factor, this->Position.z / factor };
			return Vertex(newPos);
		}

		Vertex& operator*=(const float factor)
		{
			this->Position.x *= factor;
			this->Position.y *= factor;
			this->Position.z *= factor;

			return *this;
		}

		Vertex operator-(const Vertex& a)
		{
			DirectX::XMFLOAT3 newPos = { this->Position.x - a.Position.x, this->Position.y - a.Position.y, this->Position.z - a.Position.z };
			return Vertex(newPos);
		}

		// Nested Hasher
		struct Hasher {
			std::size_t operator()(const Vertex& v) const {
				std::size_t hx = std::hash<float>()(v.Position.x);
				std::size_t hy = std::hash<float>()(v.Position.y);
				std::size_t hz = std::hash<float>()(v.Position.z);

				return hx ^ (hy << 1) ^ (hz << 2) ^ (hx >> 2) ^ (hy >> 1);
			}
		};

		// Nested Equal
		struct Equal {
			bool operator()(const Vertex& v1, const Vertex& v2) const {
				constexpr double epsilon = 1e-6f;
				return (std::abs(v1.Position.x - v2.Position.x) < epsilon) &&
					(std::abs(v1.Position.y - v2.Position.y) < epsilon) &&
					(std::abs(v1.Position.z - v2.Position.z) < epsilon);
			}
		};
	};

	struct Animation 
	{
		std::string Name;
		float Duration = 0.0f;
		uint32_t SampleCount = 0;

		Buffer TranslationBuffer;
		uint32_t TranslationSampleCount = 0;
		Buffer TranslationTimestampBuffer;

		Buffer RotationBuffer;
		uint32_t RotationSampleCount = 0;
		Buffer RotationTimestampBuffer;

		Buffer ScaleBuffer;
		uint32_t ScaleSampleCount = 0;
		Buffer ScaleTimestampBuffer;
	};

	struct AnimationPlayback 
	{
		float TimeElapsed = 0.0f;
		bool IsActive = false;
		bool IsReversed = false;
		bool HasPlayed = false;

		bool BaseCaptured = false;

		void Play()
		{
			IsActive = true;
			HasPlayed = true;
			IsReversed = false;
		}

		void PlayReverse()
		{
			IsActive = true;
			HasPlayed = true;
			IsReversed = true;
		}

		void PlayFromStart()
		{
			IsActive = true;
			HasPlayed = true;
			IsReversed = false;
			TimeElapsed = 0.0f; // explicit reset to beginning
		}

		void Reset()
		{
			IsActive = false;
			HasPlayed = false;
			TimeElapsed = 0.0f;
		}
	};

	struct PartLODTransform
	{
		DirectX::XMFLOAT3 LocalTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 LocalRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT3 LocalScale = { 1.0f, 1.0f, 1.0f };
		DirectX::XMMATRIX Parent = DirectX::XMMatrixIdentity();
		bool Captured = false;
	};

	struct Part
	{
		Part() = default;
		Part(const std::string& name)
			: Name(name) {}

		std::string Name = "";

		DirectX::XMFLOAT3 RestTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 RestRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT3 RestScale = { 1.0f, 1.0f, 1.0f };

		std::vector<PartLODTransform> LODTransforms;

		bool RestTransformCaptured = false;

		std::vector<std::unordered_map<std::string, Ref<Animation>>> LODAnimations;
		bool IsAnimated = false;

		bool Sample(const std::string& animationName, uint32_t lodIndex, float time,
			DirectX::XMMATRIX& outMatrix) const;
	};

	class Submesh
	{
	public:
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		uint32_t IndexCount;
		uint32_t VertexCount;
		std::string MaterialName;
		uint32_t PartIndex = UINT32_MAX;
	};

	struct LODGroup
	{
		LODGroup() = default;

		std::vector<Submesh> Submeshes;

		uint32_t VertexCount = 0;
		uint32_t IndexCount = 0;

		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;

		Ref<VertexBuffer> VBuffer;
		Ref<VertexBuffer> InstancedVBuffer;
		Ref<IndexBuffer> IBuffer;
		uint32_t NumberOfInstances = 0;
	};

	class Mesh 
	{
	public:
		enum class MeshType { NONE = 0, MODEL, CUBE, SPHERE, PLANET };
	public:
		Mesh();
		//Mesh(Ref<Material>& planetMaterial);
		Mesh(const std::string& filePath, Vector3 colorOverride = { 0.0, 0.0, 0.0 }, bool isInstanced = false, uint32_t maxNrOfInstanceObjects = 0);
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const DirectX::XMMATRIX& transform);
		~Mesh() = default;

		void LoadMesh(cgltf_data* data);
		void LoadMeshWithLODs(cgltf_data* data);

		bool HasLODGroups() { return mHasLODs; }
		void SetLODThresholds(std::vector<float>& updatedThreshold) { mLODThresholds = updatedThreshold; }
		std::vector<float>& GetLODThresholds() { return mLODThresholds; }

		void InvalidatePlanet();

		const std::string& GetFilePath() const { return mFilePath; }

		std::vector<Part>& GetParts() { return mParts; }

		std::vector<Vertex>& GetVertices(size_t lod) { return mLODGroups[lod]->Vertices; }
		std::vector<uint32_t>& GetIndices(size_t lod) { return mLODGroups[lod]->Indices; }

		std::vector<Submesh>& GetSubmeshes(size_t lod) { return mLODGroups[lod]->Submeshes; }
		void AddSubmesh(uint32_t indexCount, size_t LODGroupIndex = 0);
		uint32_t GetNumberOfSubmeshes(size_t lod) { return mLODGroups[lod]->Submeshes.size(); }

		const Ref<Material> GetMaterial(std::string materialName) const { if (mMaterials.find(materialName) != mMaterials.end()) return mMaterials.at(materialName); else return nullptr; }
		void SetMaterial(std::string materialName, Ref<Material> material) { mMaterials[materialName] = material; }

		void Bind(size_t lod);

		bool GetIsAnimated() const { return mIsAnimated; }
		bool HasAnimation(const std::string& name) const;
		float GetAnimationDuration(const std::string& name) const;

		bool IsInstanced() const { return mInstanced; }
		uint32_t GetNumberOfInstances(size_t LODGroupIndex) const { return mLODGroups[LODGroupIndex]->NumberOfInstances; }
		void SetInstanceData(const void* data, uint32_t size, uint32_t numberOfInstances, size_t lodIndex);

		std::vector<Ref<LODGroup>>& GetLODGroups() { return mLODGroups; }

		int32_t FindPartIndex(const std::string& name) const;
	private:
		void ProcessLODNode(const cgltf_node* node, Ref<LODGroup> lodGroup, uint32_t lodIndex, const DirectX::XMMATRIX& parentTransform, uint32_t& vertexCount, uint32_t& indexCount);

		uint32_t GetOrCreatePartIndex(const std::string& partName);
	private:
		std::string mFilePath = "";

		bool mHasLODs = false;
		std::vector<float> mLODThresholds = { 0.3f, 0.6f };
		std::vector<Ref<LODGroup>> mLODGroups;

		Vector3 mColorOverride;
		uint32_t mMaxNrOfInstanceObjects = 0;
		bool mInstanced = false;

		std::unordered_map<std::string, Ref<Material>> mMaterials;

		std::vector<Part> mParts;
		std::unordered_map<std::string, uint32_t> mPartNameToIndex;

		PrimitiveTopology mTopology = PrimitiveTopology::TRIANGLELIST;

		bool mIsAnimated = false;

		friend class Scene;
		friend class Renderer;
		friend class RendererDebug;
		friend class Primitives;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class ScriptWrappers;
		friend class PlanetSystem;
	};
}