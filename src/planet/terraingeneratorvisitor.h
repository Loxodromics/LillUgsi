#pragma once

#include "planetgenerator.h"
#include "vertexdata.h"
#include "vertexvisitor.h"
#include <glm/glm.hpp>


namespace lillugsi::planet
{
class TerrainGeneratorVisitor final : public VertexVisitor {
public:
	explicit TerrainGeneratorVisitor(const PlanetGenerator::GeneratorSettings& settings);
	void visit(std::shared_ptr<VertexData> vertex) override;

private:
	PlanetGenerator::GeneratorSettings settings;
	[[nodiscard]] float generateNoiseValue(const glm::vec3& position) const;
};

} /// namespace lillugsi::planet