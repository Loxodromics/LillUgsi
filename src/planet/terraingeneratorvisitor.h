#pragma once

#include "planetgenerator.h"
#include "vertexdata.h"
#include "vertexvisitor.h"

#include <glm/glm.hpp>
#include <FastNoise/FastNoise.h>

namespace lillugsi::planet
{
class TerrainGeneratorVisitor final : public VertexVisitor {
public:
	explicit TerrainGeneratorVisitor(const PlanetGenerator::GeneratorSettings& settings);
	void visit(std::shared_ptr<VertexData> vertex) override;

private:
	[[nodiscard]] double generateNoiseValue(const glm::dvec3& position) const;


	PlanetGenerator::GeneratorSettings settings;
	FastNoise::SmartNode<FastNoise::FractalFBm> terrainNoise;
	FastNoise::SmartNode<FastNoise::FractalFBm> terrainTypeNoise;
};

} /// namespace lillugsi::planet