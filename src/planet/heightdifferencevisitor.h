#pragma once

#include "vertexvisitor.h"
#include <spdlog/spdlog.h>

namespace lillugsi::planet {

class HeightDifferenceVisitor : public VertexVisitor {
public:
	/// Create visitor with threshold for height differences
	/// @param threshold Maximum allowed height difference between neighbors
	explicit HeightDifferenceVisitor(float threshold = 0.1f)
		: threshold(threshold) {
		spdlog::info("Height difference visitor created with threshold {}", threshold);
	}

	void visit(std::shared_ptr<VertexData> vertex) override {
		/// Get current vertex height and position for reference
		const float currentHeight = vertex->getElevation();
		const glm::vec3 currentPos = vertex->getPosition();
		
		/// Check against all neighbors
		const auto neighbors = vertex->getNeighbors();
		for (const auto& neighbor : neighbors) {
			const float neighborHeight = neighbor->getElevation();
			const float difference = std::abs(currentHeight - neighborHeight);
			
			if (difference > this->threshold) {
				spdlog::warn("Large height difference detected:");
				spdlog::warn("\tVertex at ({:.3f}, {:.3f}, {:.3f}) height: {:.3f}",
					currentPos.x, currentPos.y, currentPos.z, currentHeight);
				spdlog::warn("\tNeighbor at ({:.3f}, {:.3f}, {:.3f}) height: {:.3f}",
					neighbor->getPosition().x,
					neighbor->getPosition().y,
					neighbor->getPosition().z,
					neighborHeight);
				spdlog::warn("\tDifference: {:.3f} (threshold: {:.3f})",
					difference, this->threshold);

				float sumHeight = 0.0f;
				for (const auto& neighbor2 : neighbors) {
					sumHeight += neighbor2->getElevation();
				}
				const float averageHeight = sumHeight / static_cast<float>(neighbors.size());
				vertex->setElevation(averageHeight);
			}
		}
	}

private:
	float threshold;
};

} /// namespace lillugsi::planet