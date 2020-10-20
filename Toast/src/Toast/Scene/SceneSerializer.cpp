#include "tpch.h"
#include "SceneSerializer.h"

#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"

#include "json.hpp"

#include <DirectXMath.h>

namespace Toast {

	//static void SerializeEntity(Entity entity, )

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: mScene(scene)
	{

	}

	void SceneSerializer::SerializeScene(const std::string& filepath)
	{
		nlohmann::json j;

		mScene->mRegistry.each([&](auto entityID){
			Entity entity{ entityID, mScene.get() };

			if (!entity)
				return;

			// Tag Component
			{
				std::string tag = entity.GetComponent<TagComponent>().Tag;
				j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["tag"]["name"] = tag;
				j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["tag"]["id"] = static_cast<uint64_t>(entityID);
			}

			// Transform Component
			{
				if (entity.HasComponent<TagComponent>()) {
					DirectX::XMFLOAT4X4 mat; DirectX::XMStoreFloat4x4(&mat, entity.GetComponent<TransformComponent>().Transform);
					std::vector<float> matData;
					matData.resize(16);

					memcpy(matData.data(), &mat, sizeof(mat));
					j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["transform"] = matData;
				}
			}

			// Mesh Component
			{
				if (entity.HasComponent<MeshComponent>()) {
					auto& mc = entity.GetComponent<MeshComponent>();

					j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["mesh"]["type"] = static_cast<uint32_t>(mc.Mesh->GetType());

					if (mc.Mesh->GetType() == Mesh::MeshType::PRIMITIVE) {
						j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["mesh"]["primitive type"] = static_cast<uint32_t>(mc.Mesh->GetPrimitiveType());
					}
				}
			}

			// Sprite Render Component
			{
				if (entity.HasComponent<SpriteRendererComponent>()) {
					auto& sprite = entity.GetComponent<SpriteRendererComponent>();

					j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["sprite"]["color"] = { sprite.Color.x, sprite.Color.y, sprite.Color.z, sprite.Color.w };
				}
			}

			// Camera Component
			{
				if (entity.HasComponent<CameraComponent>()) {
					auto& cc = entity.GetComponent<CameraComponent>();

					j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["type"] = cc.Camera.GetProjectionType();

					if (cc.Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
						j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["verticalfov"] = cc.Camera.GetPerspectiveVerticalFOV();
						j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["nearclip"] = cc.Camera.GetPerspectiveNearClip();
						j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["farclip"] = cc.Camera.GetPerspectiveFarClip();
					}
					else if (cc.Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
						j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["size"] = cc.Camera.GetOrthographicSize();
						j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["nearclip"] = cc.Camera.GetOrthographicNearClip();
						j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["farclip"] = cc.Camera.GetOrthographicFarClip();
					}

					j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["aspectRatio"] = cc.Camera.GetAspecRatio();
					j["entities"][std::to_string(static_cast<uint64_t>(entityID))]["components"]["camera"]["primary"] = cc.Primary;
				}
			}

			// Native Script Component
			// TO DO
		});

		std::ofstream file(filepath, std::ios::out);
		TOAST_CORE_ASSERT(file.is_open(), "Failed to write to scene file '{0}'.", filepath);
		file << j.dump(4);
		file.close();
	}

	bool SceneSerializer::DeserializeScene(const std::string& filepath)
	{
		std::fstream file(filepath);
		TOAST_CORE_ASSERT(file.is_open(), "Failed to read from scene file '{0}'.", filepath);
		std::stringstream ss;
		ss << file.rdbuf();
		std::string scenestring = ss.str();
		nlohmann::json j = nlohmann::json::parse(scenestring);

		nlohmann::json& entities = j["entities"];

		for (auto& entity : entities) 
		{
			nlohmann::json& components = entity["components"];

			nlohmann::json& tag = components["tag"];

			uint64_t uuid = tag["id"].get<uint64_t>();
			std::string name = tag["name"];

			TOAST_CORE_INFO("Deserialized entity with ID = {0}, name = {1}", uuid, name);

			Entity deserializedEntity = mScene->CreateEntity(name);

			// Transform			
			{
				DirectX::XMFLOAT4X4 transform;
				std::vector<float> trans = components["transform"];
				memcpy(&transform, trans.data(), sizeof(transform));
				deserializedEntity.GetComponent<TransformComponent>().Transform = XMLoadFloat4x4(&transform);
			}

			// Mesh
			{
				if (components.find("mesh") != components.end()){
					nlohmann::json& mesh = components["mesh"];

					deserializedEntity.AddComponent<MeshComponent>(CreateRef<Mesh>());
					auto& mc = deserializedEntity.GetComponent<MeshComponent>();

					mc.Mesh->SetType(static_cast<Mesh::MeshType>(mesh["type"].get<uint32_t>()));
					mc.Mesh->SetPrimitiveType(static_cast<Mesh::PrimitiveType>(mesh["primitive type"]));
					mc.Mesh->CreateFromPrimitive();
				}
			}

			// Sprite Render Components
			{
				if (components.find("sprite") != components.end()) {
					nlohmann::json& sprite = components["sprite"];

					std::vector<float> c = sprite["color"];
					DirectX::XMFLOAT4 color;
					memcpy(&color, c.data(), sizeof(color));
					deserializedEntity.AddComponent<SpriteRendererComponent>(color);
				}
			}

			// Camera Components
			{
				if (components.find("camera") != components.end()){
					nlohmann::json& camera = components["camera"];

					deserializedEntity.AddComponent<CameraComponent>();
					auto& cc = deserializedEntity.GetComponent<CameraComponent>();

					cc.Primary = camera["primary"].get<bool>();
					cc.Camera.SetAspectRatio(camera["aspectRatio"].get<float>());

					if (camera["type"] == SceneCamera::ProjectionType::Perspective){
						cc.Camera.SetPerspectiveVerticalFOV(camera["verticalfov"].get<float>());
						cc.Camera.SetPerspectiveNearClip(camera["nearclip"].get<float>());
						cc.Camera.SetPerspectiveFarClip(camera["farclip"].get<float>());
					}
					else if (camera["type"] == SceneCamera::ProjectionType::Orthographic) {
						cc.Camera.SetOrthographicSize(camera["size"].get<float>());
						cc.Camera.SetOrthographicNearClip(camera["nearclip"].get<float>());
						cc.Camera.SetOrthographicFarClip(camera["farclip"].get<float>());
					}
				}
			}
		}

		return false;
	}

}