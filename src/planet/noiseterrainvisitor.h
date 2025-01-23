#pragma once

#include "vertexvisitor.h"
#include <glm/glm.hpp>
#include <random>

namespace lillugsi::planet {

/// Visitor that applies noise-based elevation changes to vertices
/// Creates randomized terrain features like mountains and valleys
class NoiseTerrainVisitor : public VertexVisitor {
public:
	/// Create visitor with given noise parameters
	explicit NoiseTerrainVisitor(float scale = 1.0f, float magnitude = 1.0f);

	void visit(std::shared_ptr<VertexData> vertex) override;

private:
	/// Scale affects the frequency of terrain features
	float scale;
	/// Magnitude affects the height of terrain features
	float magnitude;
	
	/// Random number generation for noise
	std::mt19937 rng;
	std::uniform_real_distribution<float> dist;
};

} /// namespace lillugsi::planet