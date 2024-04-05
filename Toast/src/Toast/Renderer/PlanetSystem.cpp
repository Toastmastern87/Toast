#include "tpch.h"

#include "PlanetSystem.h"

namespace Toast {

	std::mutex PlanetSystem::planetDataMutex;
	std::future<void> PlanetSystem::generationFuture;
	std::atomic<bool> PlanetSystem::newPlanetReady{ false };

}