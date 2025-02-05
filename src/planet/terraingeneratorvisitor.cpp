#include "terraingeneratorvisitor.h"

#include <spdlog/spdlog.h>

namespace lillugsi::planet
{
TerrainGeneratorVisitor::TerrainGeneratorVisitor(const PlanetGenerator::GeneratorSettings& settings)
	: settings(settings) {

	this->terrainNoise = FastNoise::New<FastNoise::FractalFBm>();
	const auto terrainNoiseSource = FastNoise::New<FastNoise::Simplex>();
	this->terrainNoise->SetSource(terrainNoiseSource);
	this->terrainNoise->SetOctaveCount( 7 );
	this->terrainNoise->SetLacunarity(2.0);
	this->terrainNoise->SetGain(0.5);
	this->terrainNoise->SetWeightedStrength(0.0);

	auto terrainTypeNoiseSource = FastNoise::New<FastNoise::Simplex>();
	this->terrainTypeNoise = FastNoise::New<FastNoise::FractalFBm>();
	this->terrainTypeNoise->SetSource(terrainNoiseSource); /// Do I need two different soruces?
	this->terrainTypeNoise->SetSource(terrainNoiseSource);
	this->terrainTypeNoise->SetOctaveCount( 7 );
	this->terrainTypeNoise->SetLacunarity(2.0);
	this->terrainTypeNoise->SetGain(0.5);
	this->terrainTypeNoise->SetWeightedStrength(0.07f);

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

	const float terrainTypeValue = this->terrainTypeNoise->GenSingle3D(
	static_cast<float>(position.x * this->settings.baseFrequency),
	static_cast<float>(position.y * this->settings.baseFrequency),
	static_cast<float>(position.z * this->settings.baseFrequency),
	this->settings.seed + 100);

	this->terrainNoise->SetGain(terrainTypeValue * 0.60f + 0.1f);

	const double noiseValue = this->terrainNoise->GenSingle3D(
		static_cast<float>(position.x * this->settings.baseFrequency),
		static_cast<float>(position.y * this->settings.baseFrequency),
		static_cast<float>(position.z * this->settings.baseFrequency),
		this->settings.seed);
	return noiseValue;
}

} /// namespace lillugsi::planet