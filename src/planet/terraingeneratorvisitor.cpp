#include "terraingeneratorvisitor.h"

#include <glm/gtc/noise.hpp>
#include <spdlog/spdlog.h>

namespace lillugsi::planet
{
TerrainGeneratorVisitor::TerrainGeneratorVisitor(const PlanetGenerator::GeneratorSettings& settings)
	: settings(settings) {
}

void TerrainGeneratorVisitor::visit(const std::shared_ptr<VertexData> vertex) {
	/// Use the vertex's normalized position as input to our noise function.
	/// This ensures:
	/// 1. Consistent noise mapping across the sphere
	/// 2. No distortion at poles or edges of faces
	/// 3. Seamless wrapping around the sphere
	const glm::vec3 position = glm::normalize(vertex->getPosition());

	/// Generate base noise value
	const float noiseValue = this->generateNoiseValue(position);

	/// Apply the noise value as an elevation change
	/// The current elevation is preserved to allow for cumulative changes
	const float newElevation = vertex->getElevation() + noiseValue;
	vertex->setElevation(newElevation);

	spdlog::trace("Applied elevation {} to vertex at position ({}, {}, {})",
		newElevation, position.x, position.y, position.z);
}

float TerrainGeneratorVisitor::generateNoiseValue(const glm::vec3& position) const {
	/// We use multiple octaves of simplex noise to create natural-looking terrain.
	/// Each octave adds finer detail to the terrain, with diminishing influence:
	/// - Lower frequencies create large-scale features (mountains, continents)
	/// - Higher frequencies add small-scale detail (hills, roughness)
	float total = 0.0f;
	float frequency = this->settings.baseFrequency;
	float amplitude = this->settings.amplitude;
	float maxValue = 0.0f;  /// Track maximum for normalization

	for (uint32_t i = 0; i < this->settings.octaves; ++i) {
		/// Sample noise at current frequency
		const float noiseValue = glm::simplex(position * frequency);

		/// Add weighted noise contribution
		total += noiseValue * amplitude;

		/// Track maximum possible value for normalization
		maxValue += amplitude;

		/// Modify frequency and amplitude for next octave
		/// - Frequency increases to add finer detail
		/// - Amplitude decreases to reduce influence of higher frequencies
		frequency *= this->settings.lacunarity;
		amplitude *= this->settings.persistence;
	}

	/// Normalize the result to ensure consistent height range
	/// regardless of the number of octaves
	return total / maxValue;
}

} /// namespace lillugsi::planet