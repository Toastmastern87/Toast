#include "tpch.h"
#include "PhysicsEngineUpdated.h"


namespace Toast {

	PhysicsEngineUpdated::PhysicsEngineUpdated()
	{
		mScene = nullptr;
	}

	void PhysicsEngineUpdated::Initialize(Scene* scene)
	{
		mScene = scene;
	}

	void PhysicsEngineUpdated::Update(double ts)
	{

	}

	double PhysicsEngineUpdated::GetAltitude(Entity& entity)
	{
		auto& registry = mScene->GetRegistry();
		auto cameraView = registry.view<CameraComponent>();

		DirectX::XMFLOAT3 worldTranslation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

		for (auto cameraEntity : cameraView)
		{
			Entity e = { cameraEntity, mScene };

			worldTranslation = e.GetComponent<CameraComponent>().Camera.GetWorldTranslation();

			break;
		}

		TransformComponent& tc = entity.GetComponent<TransformComponent>();

		float u, v;
		double radialDist;

		Planet& planet = *mScene->GetPlanet();

		TerrainData& terrainData = planet.GetTerrainData();

		WorldPosToHeightMapUV(planet, tc.Translation, worldTranslation, terrainData.Width, terrainData.Height, u, v, radialDist);

		// 2) Sample true surface radius at that UV
		//    (heightData already encodes [minAlt..maxAlt], so
		//     that value is the surface radius from center)
		double height = SampleHeightBilinear(terrainData.HeightData, terrainData.Width, terrainData.Height, u, v);

		// 3) Altitude = how far you are above that surface radius
		return radialDist - (planet.GetRadius() + height);
	}

	TerrainData PhysicsEngineUpdated::LoadTerrainData(const std::string& path, const double maxHeight, const double minHeight)
	{
		HRESULT result;

		std::wstring w;
		std::copy(path.c_str(), path.c_str() + strlen(path.c_str()), back_inserter(w));
		const WCHAR* pathWChar = w.c_str();

		DirectX::TexMetadata heightMapMetadata;
		DirectX::ScratchImage* heightMap = new DirectX::ScratchImage();

		result = DirectX::LoadFromWICFile(pathWChar, DirectX::WIC_FLAGS_NONE, &heightMapMetadata, *heightMap);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load height map!");

		TOAST_CORE_INFO("Terrain data loaded width: %d, height: %d, format: %d", heightMapMetadata.width, heightMapMetadata.height, heightMapMetadata.format);

		TerrainData td;
		td.Width = heightMapMetadata.width;
		td.Height = heightMapMetadata.height;
		td.RowPitch = heightMap->GetImage(0, 0, 0)->rowPitch;

		const uint16_t* src = reinterpret_cast<const uint16_t*>(heightMap->GetPixels());

		size_t total = td.Width * td.Height;

		td.HeightData.resize(total);
		for (size_t i = 0; i < total; ++i)
			td.HeightData[i] = ((static_cast<double>(src[i]) / MAX_INT_VALUE) * (maxHeight - minHeight)) + minHeight;

		td.Stride = td.Width;

		return td;
	}

	void PhysicsEngineUpdated::ApplyLinearImpulse(RigidBodyComponent& rbc, Vector3 impulse)
	{
		if (rbc.InvMass == 0.0)
			return;

		rbc.LinearVelocity += (impulse * rbc.InvMass);
	}

	void PhysicsEngineUpdated::ApplyGravity(double ts)
	{
		auto view = mScene->mRegistry.view<RigidBodyComponent>();
		for (auto entity : view)
		{
			RigidBodyComponent& rbc = view.get<RigidBodyComponent>(entity);

			//ApplyLinearImpulse(rbc, Vector3(0.0, -9.81 * rbc.Mass * ts, 0.0));
		}
	}

	void PhysicsEngineUpdated::WorldPosToHeightMapUV(Planet& p, const Vector3& worldPos, const Vector3& worldTranslation, int mapWidth, int mapHeight, float& outU, float& outV, double& outRadialDist)
	{
		Vector3 planetTranslation = p.GetTranslation();

		// 1) Move into planet local coordinates
		Vector3 pLocal = worldPos - planetTranslation - worldTranslation;
		//Vector3 local = Vector3::Rotate(p, PlanetSystem::GetInvRotation());
		// 3) world-space unit normal (matches nWS in the VS) 
		outRadialDist = pLocal.Length();

		Vector3 nWS = pLocal / outRadialDist;

		// 4) fetch the same basis vectors you put in the cbuffer
		const DirectX::XMFLOAT3 east = p.GetBasisLonEast();   // == BasisLonEast
		const DirectX::XMFLOAT3 north = p.GetBasisLonNorth();  // == BasisLonNorth
		const DirectX::XMFLOAT3 spinUp = p.GetBasisSpinUp();    // == BasisSpinUp

		// 5) identical math to SphereUV()
		double vx = Vector3::Dot(nWS, east);
		double vy = Vector3::Dot(nWS, spinUp);
		double vz = Vector3::Dot(nWS, north);

		double lon = std::atan2(vz, vx);          // −π … +π
		double lat = std::asin(vy);              // −π/2 … +π/2

		outU = static_cast<float>(lon * (1.0 / (2.0 * M_PI)) + 0.5);
		outV = 0.5f - static_cast<float>(lat * (1.0 / M_PI));

		// 4) Wrap U (in case of small floating drift)
		if (outU < 0.f)       outU += 1.f;
		else if (outU > 1.f)  outU -= 1.f;
	}

	double PhysicsEngineUpdated::SampleHeightBilinear(const std::vector<double>& heightData, int textureWidth, int textureHeight, float u, float v)
	{
		// Texel coordinates (floating)
		double fx = u * textureWidth - 0.5;
		double fy = v * textureHeight - 0.5;

		int x0 = (int)std::floor(fx);
		int y0 = (int)std::floor(fy);
		int x1 = (std::min)(x0 + 1, textureWidth - 1);
		int y1 = (std::min)(y0 + 1, textureHeight - 1);

		double sx = fx - x0;          // 0 … 1
		double sy = fy - y0;

		// Fetch four corners
		double h00 = heightData[y0 * textureWidth + x0];
		double h10 = heightData[y0 * textureWidth + x1];
		double h01 = heightData[y1 * textureWidth + x0];
		double h11 = heightData[y1 * textureWidth + x1];

		// Bilinear interpolation
		double h0 = h00 + (h10 - h00) * sx;
		double h1 = h01 + (h11 - h01) * sx;

		return h0 + (h1 - h0) * sy;
	}

}