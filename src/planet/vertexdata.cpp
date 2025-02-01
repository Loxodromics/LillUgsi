#include "vertexdata.h"
#include "face.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

namespace lillugsi::planet {
VertexData::VertexData(const glm::dvec3& position, size_t index)
	: position(position)
	, index(index) {
	/// Position is set in initializer list since it's constant after creation
}

void VertexData::setElevation(double newElevation) {
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

glm::dvec3 VertexData::getNormal() {
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

double VertexData::getSlope(size_t neighborIndex) {
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
	const auto currentNeighbors = this->getNeighbors();

	/// If we don't have enough neighbors to calculate a meaningful normal,
	/// we use the normalized position vector as this is correct for a sphere
	if (currentNeighbors.size() < 2) {
		this->normal = glm::normalize(this->position);
		spdlog::warn(
			"Insufficient neighbors ({}) to calculate normal, using normalized position",
			currentNeighbors.size());
		return;
	}

	/// For a sphere, the correct normal should point in the same direction as the position vector
	/// We use this reference direction to ensure consistent orientation
	const glm::dvec3 desiredDirection = glm::normalize(this->position);

	/// Calculate the normal by averaging cross products of vectors to adjacent vertices
	glm::dvec3 summedNormal(0.0f);

	for (size_t i = 0; i < currentNeighbors.size(); ++i) {
		const size_t nextIndex = (i + 1) % currentNeighbors.size();

		/// Calculate vectors from this vertex to its neighbors
		const glm::dvec3 basePos = this->position * (1.0 + this->elevation);
		const glm::dvec3 neighborPos1 = currentNeighbors[i]->getPosition()
										* (1.0 + currentNeighbors[i]->getElevation());
		const glm::dvec3 neighborPos2 = currentNeighbors[nextIndex]->getPosition()
										* (1.0 + currentNeighbors[nextIndex]->getElevation());

		/// Calculate vectors forming the triangle
		const glm::dvec3 edge1 = neighborPos1 - basePos;
		const glm::dvec3 edge2 = neighborPos2 - basePos;

		/// Calculate cross product
		glm::dvec3 triangleNormal = glm::cross(edge1, edge2);

		/// Check if this normal points in roughly the same direction as our position
		/// If not, flip it to ensure consistent orientation
		if (glm::dot(triangleNormal, desiredDirection) < 0.0f) {
			triangleNormal = -triangleNormal;
		}

		/// Only add non-zero contributions
		if (glm::length2(triangleNormal) > EPSILON) {
			summedNormal += triangleNormal;
		}
	}

	/// If we got a valid normal, normalize it
	/// Otherwise, use normalized position as fallback
	if (glm::length2(summedNormal) > EPSILON) {
		this->normal = glm::normalize(summedNormal);

		/// Double-check orientation
		if (glm::dot(this->normal, desiredDirection) < 0.0f) {
			this->normal = -this->normal;
		}
	} else {
		this->normal = desiredDirection;
		spdlog::info("Failed to calculate valid normal, falling back to normalized position");
	}

	spdlog::trace(
		"Recalculated normal for vertex at position ({}, {}, {})",
		this->position.x,
		this->position.y,
		this->position.z);
}

glm::dvec3 VertexData::calculateNormalFromFaces(
	const std::vector<std::shared_ptr<Face>>& faces,
	const std::vector<std::shared_ptr<VertexData>>& vertices) const {

	/// Start with zero vector to accumulate weighted normals
	glm::dvec3 summedNormal(0.0, 0.0, 0.0);

	/// Use position with elevation for calculations
	const glm::dvec3 elevatedPosition = this->position * (1.0 + this->elevation);

	for (const auto& face : faces) {
		/// Skip invalid faces
		if (!face) continue;

		/// Get face normal
		const glm::dvec3& faceNormal = face->getNormal();

		/// Get face vertex indices
		const auto indices = face->getVertexIndices();

		/// Find the other two vertex indices for this face
		size_t v1Index, v2Index;
		if (indices[0] == this->index) {
			v1Index = indices[1];
			v2Index = indices[2];
		} else if (indices[1] == this->index) {
			v1Index = indices[2];
			v2Index = indices[0];
		} else if (indices[2] == this->index) {
			v1Index = indices[0];
			v2Index = indices[1];
		} else {
			/// This face doesn't contain our vertex - skip it
			spdlog::warn("Face does not contain vertex {}", this->index);
			continue;
		}

		/// Calculate vectors to neighboring vertices
		const glm::dvec3 edge1 = vertices[v1Index]->getPosition() *
			(1.0 + vertices[v1Index]->getElevation()) - elevatedPosition;
		const glm::dvec3 edge2 = vertices[v2Index]->getPosition() *
			(1.0 + vertices[v2Index]->getElevation()) - elevatedPosition;

		/// Calculate angle at this vertex
		const double angle = std::acos(glm::dot(
			glm::normalize(edge1),
			glm::normalize(edge2)
		));

		/// Weight face normal by angle
		summedNormal += faceNormal * angle;
	}

	/// Check if we got any valid contribution
	if (glm::length2(summedNormal) > EPSILON) {
		return glm::normalize(summedNormal);
	}

	/// Fallback to normalized position if no valid faces
	return glm::normalize(this->position);
}

void VertexData::recalculateNormalFromFaces(
	const std::vector<std::shared_ptr<Face>>& faces,
	const std::vector<std::shared_ptr<VertexData>>& vertices) {

	this->normal = this->calculateNormalFromFaces(faces, vertices);
	this->normalDirty = false;
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
	const double elevationDiff = neighbor->elevation - this->elevation;
	double slope = elevationDiff / this->neighborDistances[neighborIndex];

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

double VertexData::calculateDistanceToNeighbor(const VertexData& neighbor) const {
	/// Calculate true 3D distance between vertices on the sphere
	return glm::length(neighbor.position - this->position);
}

} /// namespace lillugsi::planet
