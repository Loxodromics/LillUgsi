#include "planetgenerator.h"
#include "terraingeneratorvisitor.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/noise.hpp>

namespace lillugsi::planet {

PlanetGenerator::PlanetGenerator(std::shared_ptr<PlanetData> planetData)
	: planetData(std::move(planetData)) {
	spdlog::debug("Created PlanetGenerator instance");
}

void PlanetGenerator::generateTerrain() const {
	/// We iterate through all vertices in the planet mesh to apply noise-based
	/// elevation changes. This approach ensures consistent terrain generation
	/// across the entire surface, maintaining continuity at face boundaries.
	
	TerrainGeneratorVisitor visitor(this->settings);
	this->planetData->applyVertexVisitor(visitor);
	spdlog::info("Applied terrain generation to planet");
}

void PlanetGenerator::setSettings(const GeneratorSettings& settings) {
	this->settings = settings;
	spdlog::debug("Updated generator settings: frequency={}, amplitude={}, octaves={}",
		settings.baseFrequency, settings.amplitude, settings.octaves);
}

const PlanetGenerator::GeneratorSettings& PlanetGenerator::getSettings() const {
	return this->settings;
}

} /// namespace lillugsi::planet