#include "noiseterrainvisitor.h"
#include <spdlog/spdlog.h>

namespace lillugsi::planet {

NoiseTerrainVisitor::NoiseTerrainVisitor(const float scale, const float magnitude)
	: scale(scale)
	, magnitude(magnitude)
	, rng(std::random_device{}())
	, dist(-1.0f, 1.0f) {
}

void NoiseTerrainVisitor::visit(const std::shared_ptr<VertexData> vertex) {
	/// Generate elevation change based on vertex position
	/// Using position ensures consistent terrain across visits
	const glm::vec3 pos = vertex->getPosition();
	
	/// Create pseudo-random height based on position
	float height = sin(pos.x * this->scale) * cos(pos.y * this->scale) * sin(pos.z * this->scale);
	/// Add some randomness while keeping it consistent for same position
	height += this->dist(rng) * 0.1f;
	
	/// Scale height by magnitude and apply to vertex
	vertex->setElevation(height * this->magnitude);
	
	spdlog::trace("Set elevation {} at position [{}, {}, {}]",
		height * this->magnitude, pos.x, pos.y, pos.z);
}

} /// namespace lillugsi::planet