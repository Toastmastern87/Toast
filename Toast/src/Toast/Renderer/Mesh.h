#pragma once

#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/Formats.h"

#include "Toast/Core/Math/Math.h"

#include <DirectXMath.h>

#include <../cgltf/include/cgltf.h>

namespace Toast {

	struct TerrainData
	{
		size_t RowPitch;
		size_t Width;
		size_t Height;
		std::vector<double> HeightData;
	};

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
		bool IsActive = false;
		float Duration = 0.0f;
		float TimeElapsed = 0.0f;
		uint32_t SampleCount = 0;
		cgltf_animation_channel AnimationChannel;
		Buffer DataBuffer;

		Animation() = default;
		Animation(cgltf_animation_channel animationChannel)
			: AnimationChannel(animationChannel) {}

		void Play(float startTime) 
		{
			IsActive = true;
			TimeElapsed = startTime;
		}

		void Reset() 
		{
			IsActive = false;
			TimeElapsed = 0.0f;
		}
	};

	class Submesh
	{
	public:
		void OnUpdate(Timestep ts);

		uint32_t FindPosition(float animationTime, const std::string& animationName);
		DirectX::XMVECTOR InterpolateTranslation(float animationTime, const std::string& animationName);
	public:
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		std::string MaterialName;
		uint32_t IndexCount;
		uint32_t VertexCount;

		DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
		DirectX::XMFLOAT3 Translation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		DirectX::XMFLOAT3 StartTranslation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		DirectX::XMFLOAT4 Rotation = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		DirectX::XMFLOAT3 Scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

		std::string MeshName;

		bool IsAnimated = false;
		std::unordered_map<std::string, Ref<Animation>> Animations;
	};

	class Mesh 
	{
	public:
		enum class MeshType { NONE = 0, MODEL, CUBE, SPHERE, PLANET };
	public:
		Mesh();
		Mesh(bool isPlanet);
		Mesh(const std::string& filePath, const bool skyboxMesh = false, Vector3 colorOverride = { 0.0, 0.0, 0.0 }, bool isInstanced = false, uint32_t maxNrOfInstanceObjects = 0);
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const DirectX::XMMATRIX& transform);
		~Mesh() = default;

		template <typename T>
		void Set(const std::string& materialName, const std::string& cbufferName, const std::string& name, const T& value)
		{
			auto decl = FindCBufferElementDeclaration(materialName, cbufferName, name);

			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");
			if (!decl)
				return;

			if (cbufferName == "Model") 
				mModelBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
			else if (cbufferName == "Planet") 
				mPlanetBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
		}

		template <typename T>
		T& Get(const std::string& materialName, const std::string& cbufferName, const std::string& name)
		{
			auto decl = FindCBufferElementDeclaration(materialName, bufferName, name);
			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");

			if(cbufferName == "Model")
				return mModelBuffer.Read<T>(decl->GetOffset());
			else if (cbufferName == "Planet")
				return mPlanetBuffer.Read<T>(decl->GetOffset());
		}

		void OnUpdate(Timestep ts);
		void InvalidatePlanet();

		const std::string& GetFilePath() const { return mFilePath; }

		std::vector<Vertex>& GetVertices() { return mVertices; }
		std::vector<uint32_t>& GetIndices() { return mIndices; }

		std::vector<Submesh>& GetSubmeshes() { return mSubmeshes; }
		void AddSubmesh(uint32_t indexCount);
		uint32_t GetNumberOfSubmeshes() { return mSubmeshes.size(); }

		const Ref<Material> GetMaterial(std::string materialName) const { if (mMaterials.find(materialName) != mMaterials.end()) return mMaterials.at(materialName); else return nullptr; }
		void SetMaterial(std::string materialName, Ref<Material> material) { mMaterials[materialName] = material; }

		DirectX::XMMATRIX& GetLocalTransform() { return mSubmeshes[0].Transform; }
		void SetLocalTransform(DirectX::XMMATRIX& transform) { mSubmeshes[0].Transform = transform; }

		void Map(const std::string& materialName);
		void Bind(const std::string& materialName, bool environment = true);

		bool GetIsPlanet() const { return mIsPlanet; }

		void ResetAnimations();
		bool GetIsAnimated() const { return mIsAnimated; }

		bool IsInstanced() const { return mInstanced; }
		uint32_t GetNumberOfInstances() const { return mNumberOfInstances; }
		void SetInstanceData(const void* data, uint32_t size, uint32_t numberOfInstances);
	private:
		const ShaderCBufferElement* FindCBufferElementDeclaration(const std::string& materialName, const std::string& cbufferName, const std::string& name);
	private:
		std::string mFilePath = "";

		bool mInstanced = false;

		DirectX::XMMATRIX mTransform = DirectX::XMMatrixIdentity();

		std::vector<Submesh> mSubmeshes;

		Ref<VertexBuffer> mVertexBuffer;
		Ref<VertexBuffer> mInstanceVertexBuffer;
		uint32_t mNumberOfInstances = 0;
		Ref<IndexBuffer> mIndexBuffer;
		std::unordered_map<std::string, Ref<Material>> mMaterials;

		uint32_t mVertexCount = 0;
		uint32_t mIndexCount = 0;

		std::vector<Vertex> mVertices;
		std::vector<uint32_t> mIndices;

		PrimitiveTopology mTopology = PrimitiveTopology::TRIANGLELIST;

		Ref<ConstantBuffer> mModelCBuffer, mPlanetCBuffer;
		Buffer mModelBuffer, mPlanetBuffer;

		bool mIsPlanet = false;

		bool mIsAnimated = false;

		//std::unordered_map<Vertex, uint32_t, PlanetSystem::VertexHasher, PlanetSystem::VertexEquality> vertexMap;

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