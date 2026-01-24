#include "torusmesh.h"
#include "tangentcalculator.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace lillugsi::rendering {

TorusMesh::TorusMesh(
	float majorRadius,
	float minorRadius,
	uint32_t majorSegments,
	uint32_t minorSegments
)
	: majorRadius(majorRadius)
	, minorRadius(minorRadius)
	, majorSegments(std::clamp(majorSegments, 3u, 256u))
	, minorSegments(std::clamp(minorSegments, 3u, 256u))
{
	/// Validate parameters
	if (majorRadius <= 0.0f || minorRadius <= 0.0f) {
		spdlog::error("TorusMesh: Both radii must be positive (majorRadius={}, minorRadius={})",
			majorRadius, minorRadius);
		this->majorRadius = std::abs(majorRadius);
		this->minorRadius = std::abs(minorRadius);
	}

	/// Warn if self-intersecting
	if (this->majorRadius <= this->minorRadius) {
		spdlog::warn("TorusMesh: majorRadius ({}) <= minorRadius ({}) will cause self-intersection",
			this->majorRadius, this->minorRadius);
	}

	/// Log clamped segment counts if adjusted
	if (this->majorSegments != majorSegments || this->minorSegments != minorSegments) {
		spdlog::debug("TorusMesh: Clamped segments from ({}, {}) to ({}, {})",
			majorSegments, minorSegments, this->majorSegments, this->minorSegments);
	}
}

void TorusMesh::generateGeometry() {
	spdlog::debug("TorusMesh: Generating geometry with majorRadius={}, minorRadius={}, majorSegments={}, minorSegments={}",
		this->majorRadius, this->minorRadius, this->majorSegments, this->minorSegments);

	this->vertices.clear();
	this->indices.clear();

	/// Reserve memory for vertices
	const uint32_t vertexCount = (this->majorSegments + 1) * (this->minorSegments + 1);
	this->vertices.reserve(vertexCount);

	/// Generate vertices
	const float kTwoPi = glm::two_pi<float>();

	for (uint32_t i = 0; i <= this->majorSegments; ++i) {
		const float u = kTwoPi * static_cast<float>(i) / static_cast<float>(this->majorSegments);

		for (uint32_t j = 0; j <= this->minorSegments; ++j) {
			const float v = kTwoPi * static_cast<float>(j) / static_cast<float>(this->minorSegments);

			Vertex vertex;
			vertex.position = this->computePosition(u, v);
			vertex.normal = this->computeNormal(u, v);
			vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
			vertex.tangent = glm::vec3(0.0f); /// Will be computed by TangentCalculator
			vertex.texCoord = this->applyTextureTiling(glm::vec2(
				static_cast<float>(i) / static_cast<float>(this->majorSegments),
				static_cast<float>(j) / static_cast<float>(this->minorSegments)
			));

			this->vertices.push_back(vertex);
		}
	}

	/// Generate indices for quads (two triangles per quad)
	const uint32_t quadCount = this->majorSegments * this->minorSegments;
	this->indices.reserve(quadCount * 6);

	for (uint32_t i = 0; i < this->majorSegments; ++i) {
		for (uint32_t j = 0; j < this->minorSegments; ++j) {
			/// Compute indices for the four corners of this quad
			const uint32_t i0 = i * (this->minorSegments + 1) + j;
			const uint32_t i1 = (i + 1) * (this->minorSegments + 1) + j;
			const uint32_t i2 = i * (this->minorSegments + 1) + (j + 1);
			const uint32_t i3 = (i + 1) * (this->minorSegments + 1) + (j + 1);

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

	/// Calculate tangents from triangles
	TangentCalculator::calculateTangents(this->vertices, this->indices);

	spdlog::debug("TorusMesh: Generated {} vertices and {} indices",
		this->vertices.size(), this->indices.size());

	this->markBuffersDirty();
}

glm::vec3 TorusMesh::computePosition(float u, float v) const {
	const float r = this->majorRadius + this->minorRadius * std::cos(v);
	return glm::vec3(
		r * std::cos(u),
		r * std::sin(u),
		this->minorRadius * std::sin(v)
	);
}

glm::vec3 TorusMesh::computeNormal(float u, float v) const {
	/// Analytical normal for torus - perpendicular to surface
	return glm::normalize(glm::vec3(
		std::cos(v) * std::cos(u),
		std::cos(v) * std::sin(u),
		std::sin(v)
	));
}

} /// namespace lillugsi::rendering
