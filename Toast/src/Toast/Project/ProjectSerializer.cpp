#include "tpch.h"
#include "ProjectSerializer.h"

namespace YAML {

	template<>
	struct convert<Toast::UUID>
	{
		static Node encode(const Toast::UUID& uuid)
		{
			Node node;
			node.push_back((uint64_t)uuid);
			return node;
		}

		static bool decode(const Node& node, Toast::UUID& uuid)
		{
			uuid = node.as<uint64_t>();
			return true;
		}
	};
}

namespace Toast {

	void ProjectSerializer::Serialize(const std::string& filepath)
	{
		TOAST_CORE_ASSERT(mProject, "ProjectSerializer has no project!");

		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "Project" << YAML::Value << mProject->mName;
		out << YAML::Key << "Version" << YAML::Value << 1;

		out << YAML::Key << "ActiveScene" << YAML::Value << mProject->mActiveSceneID;

		out << YAML::Key << "Scenes" << YAML::Value;
		out << YAML::BeginSeq;

		for (const auto& [id, entry] : mProject->mScenes)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "ID" << YAML::Value << entry.Id;
			out << YAML::Key << "Path" << YAML::Value << entry.Path.generic_string(); // store relative
			out << YAML::EndMap;
		}

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		if (!fout.is_open())
		{
			TOAST_CORE_ERROR("Failed to write project file: %s", filepath.c_str());
			return;
		}

		fout << out.c_str();
		TOAST_CORE_TRACE("Serialized project '%s' -> %s", mProject->mName.c_str(), filepath.c_str());
	}
	 
	bool ProjectSerializer::Deserialize(const std::string& filepath)
	{
		TOAST_CORE_ASSERT(mProject, "ProjectSerializer has no project!");

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath);
		}
		catch (const YAML::ParserException& ex)
		{
			TOAST_CORE_ERROR("Failed to deserialize project %s\n\t%s", filepath.c_str(), ex.what());
			return false;
		}
		catch (const std::exception& ex)
		{
			TOAST_CORE_ERROR("Failed to read project %s\n\t%s", filepath.c_str(), ex.what());
			return false;
		}

		if (!data["Project"])
			return false;

		// If you want versioning:
		const int version = data["Version"] ? data["Version"].as<int>() : 1;
		if (version != 1)
		{
			TOAST_CORE_ERROR("Unsupported project version %d in %s", version, filepath.c_str());
			return false;
		}

		mProject->mName = data["Project"].as<std::string>();

		mProject->mPath = std::filesystem::absolute(std::filesystem::path(filepath)).parent_path();

		mProject->mScenes.clear();

		if (data["ActiveScene"])
			mProject->mActiveSceneID = data["ActiveScene"].as<UUID>();
		else
			mProject->mActiveSceneID = UUID{};

		auto scenes = data["Scenes"];
		if (scenes && scenes.IsSequence())
		{
			for (auto sceneNode : scenes)
			{
				if (!sceneNode["ID"] || !sceneNode["Path"])
					continue;

				const UUID ID = sceneNode["ID"].as<UUID>();
				const std::string relPathStr = sceneNode["Path"].as<std::string>();

				ProjectSceneEntry entry;
				entry.Id = ID;
				entry.Path = std::filesystem::path(relPathStr);

				mProject->mScenes[entry.Id] = entry;
			}
		}

		// If active scene is missing or not found, choose a sensible fallback:
		if (mProject->mScenes.find(mProject->mActiveSceneID) == mProject->mScenes.end())
		{
			if (!mProject->mScenes.empty())
				mProject->mActiveSceneID = mProject->mScenes.begin()->first;
		}

		TOAST_CORE_TRACE("Deserialized project '%s' (%zu scenes) from %s", mProject->mName.c_str(), mProject->mScenes.size(), filepath.c_str());

		return true;
	}

}