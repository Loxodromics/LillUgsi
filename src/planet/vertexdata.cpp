#include "vertexdata.h"
#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

namespace lillugsi::planet {
VertexData::VertexData(const glm::vec3& position)
	: position(position) {
	/// Position is set in initializer list since it's constant after creation
}

void VertexData::setElevation(float newElevation) {
	/// Only update and trigger recalculations if elevation actually changes
	if (std::abs(this->elevation - newElevation) > EPSILON) {
		this->elevation = newElevation;
		/// Mark slopes as dirty since they depend on elevation
		this->markSlopesDirty();
		/// Mark our normal as dirty since it depends on elevation
		this->markNormalDirty();
		/// Notify neighbors their normals need updating
		/// We do this because our elevation change affects their normals
		this->notifyNeighborsNormalsDirty();
	}
}

glm::vec3 VertexData::getNormal() {
	/// Check if we need to recalculate the normal
	/// This lazy evaluation approach ensures we only recalculate when necessary
	if (this->normalDirty) {
		this->recalculateNormal();
		this->normalDirty = false;
	}
	return this->normal;
}

void VertexData::markNormalDirty() {
	/// Mark normal as needing recalculation
	/// We use this flag to implement lazy evaluation of normals
	this->normalDirty = true;
}

void VertexData::notifyNeighborsNormalsDirty() const {
	/// Notify all neighbors that their normals need recalculation
	/// We do this because our elevation change affects their surface normals
	for (const auto& weakNeighbor : this->neighbors) {
		if (const auto neighbor = weakNeighbor.lock()) {
			neighbor->markNormalDirty();
		}
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

	spdlog::trace("Added new neighbor to vertex");
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
	/// Get the current neighbors that will be used for normal calculation
	/// We do this first to avoid locking weak_ptrs multiple times
	const auto currentNeighbors = this->getNeighbors();

	/// If we don't have enough neighbors to calculate a meaningful normal,
	/// we fall back to using the normalized position vector
	/// This ensures we always have a valid normal, even during mesh construction
	if (currentNeighbors.size() < 2) {
		this->normal = glm::normalize(this->position);
		spdlog::warn("Insufficient neighbors ({}) to calculate normal, using normalized position",
			currentNeighbors.size());
		return;
	}

	/// Calculate the normal by averaging cross products of vectors to adjacent vertices
	/// We use each pair of consecutive neighbors to form triangles with this vertex
	/// This approach:
	/// 1. Handles irregular vertex arrangements
	/// 2. Produces smooth normals that account for local surface curvature
	/// 3. Weights each triangle's contribution equally
	glm::vec3 summedNormal(0.0f);

	/// For each consecutive pair of neighbors, calculate their contribution to the normal
	for (size_t i = 0; i < currentNeighbors.size(); ++i) {
		/// Get the next neighbor index, wrapping around to 0 at the end
		const size_t nextIndex = (i + 1) % currentNeighbors.size();

		/// Calculate vectors from this vertex to its neighbors
		/// We incorporate elevation by adding it along the base position vector
		const glm::vec3 basePos = this->position * (1.0f + this->elevation);
		const glm::vec3 neighborPos1 = currentNeighbors[i]->getPosition() *
			(1.0f + currentNeighbors[i]->getElevation());
		const glm::vec3 neighborPos2 = currentNeighbors[nextIndex]->getPosition() *
			(1.0f + currentNeighbors[nextIndex]->getElevation());

		/// Calculate vectors forming the triangle
		const glm::vec3 edge1 = neighborPos1 - basePos;
		const glm::vec3 edge2 = neighborPos2 - basePos;

		/// Calculate the cross product of these edges
		/// The cross product gives us a vector perpendicular to both edges,
		/// which is the normal of the triangle formed by these three points
		const glm::vec3 triangleNormal = glm::cross(edge1, edge2);

		/// Only add non-zero contributions to avoid numerical issues
		/// This can happen if two vertices are at the same position
		if (glm::length2(triangleNormal) > EPSILON) {
			summedNormal += triangleNormal;
		}
	}

	/// If we got a valid normal (non-zero length), normalize it
	/// Otherwise, fall back to normalized position as a safety measure
	if (glm::length2(summedNormal) > EPSILON) {
		this->normal = glm::normalize(summedNormal);
	} else {
		this->normal = glm::normalize(this->position);
		spdlog::warn("Failed to calculate valid normal, falling back to normalized position");
	}

	spdlog::trace("Recalculated normal for vertex at position ({}, {}, {})",
		this->position.x, this->position.y, this->position.z);
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
