#include "vertexdata.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <glm/geometric.hpp>

namespace lillugsi::planet {
VertexData::VertexData(const glm::vec3& position)
	: position(position) {
	/// Position is set in initializer list since it's constant after creation
}

void VertexData::setElevation(float newElevation) {
	/// Only update and trigger recalculations if elevation actually changes
	if (std::abs(this->elevation - newElevation) > EPSILON) {
		this->elevation = newElevation;
		this->markSlopesDirty();
		this->recalculateNormal();
	}
}

void VertexData::addNeighbor(const std::shared_ptr<VertexData>& neighbor) {
	/// Skip if neighbor is null or already exists
	if (!neighbor) {
		spdlog::warn("Attempted to add null neighbor to vertex");
		return;
	}

	/// Check if neighbor already exists to avoid duplicates
	for (const auto& existingNeighbor : this->neighbors) {
		if (auto locked = existingNeighbor.lock()) {
			if (locked == neighbor) {
				spdlog::debug("Neighbor already exists for vertex");
				return;
			}
		}
	}

	/// Add new neighbor and initialize associated data
	this->neighbors.push_back(neighbor);
	this->neighborDistances.push_back(calculateDistanceToNeighbor(*neighbor));
	this->neighborSlopes.push_back(0.0f);
	this->slopeDirtyFlags.push_back(true);

	spdlog::debug("Added new neighbor to vertex");
}

std::vector<std::shared_ptr<VertexData>> VertexData::getNeighbors() const {
	std::vector<std::shared_ptr<VertexData>> result;
	result.reserve(this->neighbors.size());

	/// Convert weak_ptrs to shared_ptrs, filtering out expired references
	for (const auto& weakNeighbor : this->neighbors) {
		if (auto neighbor = weakNeighbor.lock()) {
			result.push_back(neighbor);
		}
	}

	return result;
}

float VertexData::getSlope(size_t neighborIndex) {
	/// Ensure index is valid
	if (neighborIndex >= neighbors.size()) {
		spdlog::error("Invalid neighbor index {} requested", neighborIndex);
		return 0.0f;
	}

	/// Calculate slope if dirty or neighbor has expired
	if (this->slopeDirtyFlags[neighborIndex] || this->neighbors[neighborIndex].expired()) {
		this->calculateSlope(neighborIndex);
		this->slopeDirtyFlags[neighborIndex] = false;
	}

	return neighborSlopes[neighborIndex];
}

void VertexData::recalculateNormal() {
	/// We calculate the normal by averaging the cross products of vectors
	/// to adjacent vertices. This gives us a normal vector that represents
	/// the average surface orientation at this point.

	glm::vec3 summedNormal(0.0f);
	const auto currentNeighbors = getNeighbors();

	/// Need at least 2 neighbors to calculate a meaningful normal
	if (currentNeighbors.size() < 2) {
		spdlog::warn("Insufficient neighbors to calculate normal");
		return;
	}

	/// For each consecutive pair of neighbors, calculate cross product
	for (size_t i = 0; i < currentNeighbors.size(); ++i) {
		const size_t nextIndex = (i + 1) % currentNeighbors.size();

		glm::vec3 v1 = currentNeighbors[i]->position - this->position;
		glm::vec3 v2 = currentNeighbors[nextIndex]->position - this->position;

		summedNormal += glm::cross(v1, v2);
	}

	/// Normalize the result to get a unit normal vector
	this->normal = glm::normalize(summedNormal);
}

void VertexData::clearNeighbors() {
	this->neighbors.clear();
	this->neighborDistances.clear();
	this->neighborSlopes.clear();
	this->slopeDirtyFlags.clear();
}

void VertexData::calculateSlope(size_t neighborIndex) {
	/// Get shared_ptr to neighbor, handling expired case
	const auto neighbor = this->neighbors[neighborIndex].lock();
	if (!neighbor) {
		spdlog::warn("Neighbor expired when calculating slope");
		this->neighborSlopes[neighborIndex] = 0.0f;
		return;
	}

	/// Calculate slope using elevation difference and stored distance
	const float elevationDiff = neighbor->elevation - this->elevation;
	float slope = elevationDiff / this->neighborDistances[neighborIndex];

	this->neighborSlopes[neighborIndex] = slope;
	spdlog::trace("Calculated slope {} for neighbor {}", slope, neighborIndex);
}

void VertexData::markSlopesDirty() {
	/// Mark all slopes as needing recalculation
	std::fill(slopeDirtyFlags.begin(), slopeDirtyFlags.end(), true);

	/// Notify neighbors that their slopes to us need recalculation
	for (const auto& weakNeighbor : this->neighbors) {
		if (const auto neighbor = weakNeighbor.lock()) {
			neighbor->markSlopeDirty(this);
		}
	}
}

void VertexData::markSlopeDirty(const VertexData* neighbor) {
	/// Find the index of the specified neighbor
	for (size_t i = 0; i < this->neighbors.size(); ++i) {
		if (auto locked = this->neighbors[i].lock()) {
			if (locked.get() == neighbor) {
				this->slopeDirtyFlags[i] = true;
				return;
			}
		}
	}
}

float VertexData::calculateDistanceToNeighbor(const VertexData& neighbor) const {
	/// Calculate true 3D distance between vertices on the sphere
	return glm::length(neighbor.position - this->position);
}

} /// namespace lillugsi::planet
