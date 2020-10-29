#pragma once

#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/Buffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Primitives.h"

#include <DirectXMath.h>

namespace Toast {
	struct Vertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 Tangent;
		DirectX::XMFLOAT3 Binormal;
		DirectX::XMFLOAT2 Texcoord;
	};

	class Mesh 
	{
	public:
		enum class MeshType { NONE = 0, PRIMITIVE = 1, MODEL = 2 };
		enum class PrimitiveType { NONE = 0, PLANET = 1, CUBE = 2, ICOSPHERE = 3, GRID = 4 };
	public:
		Mesh();
		~Mesh();

		void OnUpdate(Timestep ts);

		void CreateFromFile();
		void CreateFromPrimitive();

		const bool IsMeshActive() const { return mMeshActive; }

		const MeshType GetType() const { return mMeshType; }
		void SetType(MeshType type) { mMeshType = type; }

		const PrimitiveType GetPrimitiveType() const { return mPrimitiveType; }
		void SetPrimitiveType(PrimitiveType type) { mPrimitiveType = type; }

		const Ref<Shader> GetMeshShader() const { return mMeshShader; }
		const Ref<VertexBuffer> GetVertexBuffer() const { return mVertexBuffer; }
		const Ref<IndexBuffer> GetIndexBuffer() const { return mIndexBuffer; }
		const Ref<BufferLayout> GetLayout() const { return mLayout; }

		const int16_t GetGridSize() const { return mGridSize; }
		void SetGridSize(int16_t size) { mGridSize = size; }

		const int8_t GetSubdivisons() const { return mIcosphereDivision; }
		void SetSubdivisons(int8_t divisions) { mIcosphereDivision = divisions; }
	private:
		Ref<VertexBuffer> mVertexBuffer;
		Ref<IndexBuffer> mIndexBuffer;
		Ref<BufferLayout> mLayout;
		Ref<Shader> mMeshShader;

		std::vector<Vertex> mVertices;
		std::vector<uint32_t> mIndices;

		bool mMeshActive = false;
		MeshType mMeshType = MeshType::NONE;
		PrimitiveType mPrimitiveType = PrimitiveType::NONE;

		int16_t mGridSize = 5;
		int8_t mIcosphereDivision = 0;
	};
}