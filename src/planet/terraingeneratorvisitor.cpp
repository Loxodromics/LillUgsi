#include "terraingeneratorvisitor.h"

#include <glm/gtc/noise.hpp>
#include <spdlog/spdlog.h>
#include <cmath>

namespace lillugsi::planet
{
TerrainGeneratorVisitor::TerrainGeneratorVisitor(const PlanetGenerator::GeneratorSettings& settings)
	: settings(settings) {
	this->noise = FastNoise::New<FastNoise::Simplex>();
}

void TerrainGeneratorVisitor::visit(const std::shared_ptr<VertexData> vertex) {
	/// Use the vertex's normalized position as input to our noise function.
	/// This ensures:
	/// 1. Consistent noise mapping across the sphere
	/// 2. No distortion at poles or edges of faces
	/// 3. Seamless wrapping around the sphere
	const glm::dvec3 position = glm::normalize(vertex->getPosition());

	const double noiseValue = this->generateNoiseValue(position);

	/// Apply the noise value as an elevation change
	/// The current elevation is preserved to allow for cumulative changes
	const double newElevation = vertex->getElevation() + noiseValue;
	vertex->setElevation(noiseValue);

	spdlog::debug("Applied elevation {} to vertex at position ({}, {}, {})",
		newElevation, position.x, position.y, position.z);
}

double TerrainGeneratorVisitor::generateNoiseValue(const glm::dvec3& position) const {
	/// Generate base noise value
	// const float noiseValue = sin(position.x * 8.0f) * sin(position.y * 8.0f) * sin(position.z * 8.0f);
	// const float noiseValue = 1.5f;
	const double noiseValue = this->noise->GenSingle3D(position.x, position.y, position.z, this->settings.seed);
	return noiseValue;
}

} /// namespace lillugsi::planet