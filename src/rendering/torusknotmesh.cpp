#include "torusknotmesh.h"
#include "tangentcalculator.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/constants.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <numeric>

namespace lillugsi::rendering {

TorusKnotMesh::TorusKnotMesh(
	float majorRadius,
	float minorRadius,
	uint32_t p,
	uint32_t q,
	uint32_t segments,
	uint32_t tubeSegments
)
	: majorRadius(majorRadius)
	, minorRadius(minorRadius)
	, p(std::max(p, 2u))
	, q(std::max(q, 2u))
	, segments(std::clamp(segments, 32u, 1024u))
	, tubeSegments(std::clamp(tubeSegments, 8u, 64u))
{
	/// Validate parameters
	if (majorRadius <= 0.0f || minorRadius <= 0.0f) {
		spdlog::error("TorusKnotMesh: Both radii must be positive (majorRadius={}, minorRadius={})",
			majorRadius, minorRadius);
		this->majorRadius = std::abs(majorRadius);
		this->minorRadius = std::abs(minorRadius);
	}

	/// Ensure p >= 2 and q >= 2
	if (this->p != p || this->q != q) {
		spdlog::debug("TorusKnotMesh: Clamped winding numbers from ({}, {}) to ({}, {})",
			p, q, this->p, this->q);
	}

	/// Check if p and q are coprime
	const uint32_t gcdValue = std::gcd(this->p, this->q);
	if (gcdValue != 1) {
		spdlog::warn("TorusKnotMesh: p={} and q={} are not coprime (gcd={}), knot may not be simple",
			this->p, this->q, gcdValue);
	}

	/// Log clamped segment counts if adjusted
	if (this->segments != segments || this->tubeSegments != tubeSegments) {
		spdlog::debug("TorusKnotMesh: Clamped segments from ({}, {}) to ({}, {})",
			segments, tubeSegments, this->segments, this->tubeSegments);
	}
}

void TorusKnotMesh::generateGeometry() {
	spdlog::debug("TorusKnotMesh: Generating geometry with p={}, q={}, segments={}, tubeSegments={}",
		this->p, this->q, this->segments, this->tubeSegments);

	this->vertices.clear();
	this->indices.clear();

	/// Reserve memory for vertices
	const uint32_t vertexCount = (this->segments + 1) * (this->tubeSegments + 1);
	this->vertices.reserve(vertexCount);

	/// Generate vertices
	const float kTwoPi = glm::two_pi<float>();

	for (uint32_t i = 0; i <= this->segments; ++i) {
		const float t = kTwoPi * static_cast<float>(i) / static_cast<float>(this->segments);

		/// Compute position and Frenet frame at this point on the knot
		const glm::vec3 knotPos = this->computeKnotPosition(t);
		const auto [tangent, normal, binormal] = this->computeFrenetFrame(t);

		for (uint32_t j = 0; j <= this->tubeSegments; ++j) {
			const float theta = kTwoPi * static_cast<float>(j) / static_cast<float>(this->tubeSegments);

			/// Compute tube offset in the normal-binormal plane
			const glm::vec3 tubeOffset = this->minorRadius * (
				std::cos(theta) * normal +
				std::sin(theta) * binormal
			);

			Vertex vertex;
			vertex.position = knotPos + tubeOffset;
			vertex.normal = glm::vec3(0.0f); /// Will be computed by computeAveragedNormals
			vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
			vertex.tangent = glm::vec3(0.0f); /// Will be computed by TangentCalculator
			vertex.texCoord = this->applyTextureTiling(glm::vec2(
				static_cast<float>(i) / static_cast<float>(this->segments),
				static_cast<float>(j) / static_cast<float>(this->tubeSegments)
			));

			this->vertices.push_back(vertex);
		}
	}

	/// Generate indices for quads (two triangles per quad)
	const uint32_t quadCount = this->segments * this->tubeSegments;
	this->indices.reserve(quadCount * 6);

	for (uint32_t i = 0; i < this->segments; ++i) {
		for (uint32_t j = 0; j < this->tubeSegments; ++j) {
			/// Compute indices for the four corners of this quad
			const uint32_t i0 = i * (this->tubeSegments + 1) + j;
			const uint32_t i1 = (i + 1) * (this->tubeSegments + 1) + j;
			const uint32_t i2 = i * (this->tubeSegments + 1) + (j + 1);
			const uint32_t i3 = (i + 1) * (this->tubeSegments + 1) + (j + 1);

			/// First triangle (counter-clockwise winding)
			this->indices.push_back(i0);
			this->indices.push_back(i2);
			this->indices.push_back(i1);

			/// Second triangle (counter-clockwise winding)
			this->indices.push_back(i1);
			this->indices.push_back(i2);
			this->indices.push_back(i3);
		}
	}

	/// Compute face-averaged normals
	this->computeAveragedNormals();

	/// Calculate tangents from triangles
	TangentCalculator::calculateTangents(this->vertices, this->indices);

	spdlog::debug("TorusKnotMesh: Generated {} vertices and {} indices",
		this->vertices.size(), this->indices.size());

	this->markBuffersDirty();
}

glm::vec3 TorusKnotMesh::computeKnotPosition(float t) const {
	/// Parametric equations for torus knot
	const float r = this->majorRadius + this->minorRadius * std::cos(static_cast<float>(this->q) * t);
	return glm::vec3(
		r * std::cos(static_cast<float>(this->p) * t),
		r * std::sin(static_cast<float>(this->p) * t),
		this->minorRadius * std::sin(static_cast<float>(this->q) * t)
	);
}

glm::vec3 TorusKnotMesh::computeTangent(float t) const {
	/// Compute tangent using finite differences
	constexpr float epsilon = 0.001f;
	const glm::vec3 p0 = this->computeKnotPosition(t - epsilon);
	const glm::vec3 p1 = this->computeKnotPosition(t + epsilon);
	return glm::normalize(p1 - p0);
}

std::tuple<glm::vec3, glm::vec3, glm::vec3>
TorusKnotMesh::computeFrenetFrame(float t, float epsilon) const {
	/// Compute positions for finite difference approximation
	const glm::vec3 p0 = this->computeKnotPosition(t - epsilon);
	const glm::vec3 pCenter = this->computeKnotPosition(t);
	const glm::vec3 p1 = this->computeKnotPosition(t + epsilon);

	/// Compute tangent (first derivative)
	glm::vec3 tangent = glm::normalize(p1 - p0);

	/// Compute second derivative (curvature direction)
	const glm::vec3 d2p = (p1 - 2.0f * pCenter + p0) / (epsilon * epsilon);

	/// Compute normal (perpendicular to tangent, toward center of curvature)
	/// Remove tangent component from d2p to get perpendicular component
	glm::vec3 normal = d2p - tangent * glm::dot(d2p, tangent);

	/// Handle degenerate case (straight segment or inflection point)
	if (glm::length(normal) < 0.001f) {
		/// Choose arbitrary perpendicular vector
		if (std::abs(tangent.x) < 0.9f) {
			normal = glm::cross(tangent, glm::vec3(1.0f, 0.0f, 0.0f));
		} else {
			normal = glm::cross(tangent, glm::vec3(0.0f, 1.0f, 0.0f));
		}
	}
	normal = glm::normalize(normal);

	/// Compute binormal (perpendicular to both tangent and normal)
	glm::vec3 binormal = glm::normalize(glm::cross(tangent, normal));

	/// Recompute normal to ensure orthogonality (Gram-Schmidt orthogonalization)
	normal = glm::cross(binormal, tangent);

	return {tangent, normal, binormal};
}

void TorusKnotMesh::computeAveragedNormals() {
	/// Initialize all normals to zero
	for (auto& vertex : this->vertices) {
		vertex.normal = glm::vec3(0.0f);
	}

	/// Accumulate face normals to vertices (area-weighted)
	for (size_t i = 0; i < this->indices.size(); i += 3) {
		const uint32_t idx0 = this->indices[i];
		const uint32_t idx1 = this->indices[i + 1];
		const uint32_t idx2 = this->indices[i + 2];

		const glm::vec3& p0 = this->vertices[idx0].position;
		const glm::vec3& p1 = this->vertices[idx1].position;
		const glm::vec3& p2 = this->vertices[idx2].position;

		/// Compute face normal (cross product gives area-weighted normal)
		const glm::vec3 edge1 = p1 - p0;
		const glm::vec3 edge2 = p2 - p0;
		const glm::vec3 faceNormal = glm::cross(edge1, edge2);

		/// Accumulate to all three vertices
		this->vertices[idx0].normal += faceNormal;
		this->vertices[idx1].normal += faceNormal;
		this->vertices[idx2].normal += faceNormal;
	}

	/// Normalize all vertex normals
	for (auto& vertex : this->vertices) {
		if (glm::length(vertex.normal) > 0.001f) {
			vertex.normal = glm::normalize(vertex.normal);
		}
	}
}

} /// namespace lillugsi::rendering
