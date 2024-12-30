#include "icospheremesh.h"
#include <spdlog/spdlog.h>
// #include <glm/gtx/normalize.hpp>
#include <cmath>

namespace lillugsi::rendering {

IcosphereMesh::IcosphereMesh(float radius, uint32_t subdivisions) 
	: radius(radius)
	, subdivisions(subdivisions) {
	/// Validate input parameters to ensure we create a valid mesh
	if (radius <= 0.0f) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Icosphere radius must be positive",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Cap subdivisions to prevent excessive geometry
	/// Each subdivision level quadruples the number of triangles
	/// At level 8, we'd have 327,680 triangles which is excessive for most uses
	static constexpr uint32_t MaxSubdivisions = 7;
	if (subdivisions > MaxSubdivisions) {
		spdlog::warn("Capping icosphere subdivisions from {} to {}", 
			subdivisions, MaxSubdivisions);
		this->subdivisions = MaxSubdivisions;
	}

	spdlog::debug("Creating icosphere with radius {} and {} subdivisions", 
		radius, this->subdivisions);
}

void IcosphereMesh::generateGeometry() {
	this->initializeBaseIcosahedron();
}

void IcosphereMesh::initializeBaseIcosahedron() {
	/// Clear any existing geometry
	this->vertices.clear();
	this->indices.clear();
	this->midpointCache.clear();

	/// We construct the icosahedron using the golden ratio
	/// phi φ is the golden ratio (≈ 1.618033988749895)
	/// We use it because it creates the most uniform division of a sphere
	/// When we place vertices using the golden ratio, they naturally form
	/// equilateral triangles that are as close to identical as possible
	const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;

	/// invLen is the inverse of the length of the vector (1, φ)
	/// We use this to normalize our vertices to create a unit icosahedron
	/// By normalizing with this value, all vertices will lie on a unit sphere
	/// The formula comes from:
	/// 1. Our base vectors have components of (0,±1,±φ), (±1,±φ,0), or (±φ,0,±1)
	/// 2. To normalize these vectors, we divide by sqrt(1² + φ²) = sqrt(1 + φ²)
	/// 3. We precompute the inverse for efficiency
	const float invLen = 1.0f / std::sqrt(phi * phi + 1.0f);

	/// Scale factor to achieve desired radius
	/// The initial vertices form a unit icosahedron, so we scale by radius
	const float scale = this->radius;

	/// Generate the 12 vertices of the icosahedron
	/// These vertices form three orthogonal golden rectangles
	/// We start with normalized coordinates and then scale to radius
	const std::array<glm::vec3, 12> baseVertices = {{
		glm::vec3(-invLen,  phi * invLen,  0.0f) * scale,       /// Top pentagon
		glm::vec3( invLen,  phi * invLen,  0.0f) * scale,
		glm::vec3(-invLen, -phi * invLen,  0.0f) * scale,       /// Bottom pentagon
		glm::vec3( invLen, -phi * invLen,  0.0f) * scale,
		glm::vec3(0.0f,   -invLen,  phi * invLen) * scale,      /// Middle vertices
		glm::vec3(0.0f,    invLen,  phi * invLen) * scale,
		glm::vec3(0.0f,   -invLen, -phi * invLen) * scale,
		glm::vec3(0.0f,    invLen, -phi * invLen) * scale,
		glm::vec3( phi * invLen,  0.0f, -invLen) * scale,
		glm::vec3( phi * invLen,  0.0f,  invLen) * scale,
		glm::vec3(-phi * invLen,  0.0f, -invLen) * scale,
		glm::vec3(-phi * invLen,  0.0f,  invLen) * scale
	}};

	/// Convert positions to vertices with normals
	/// For a sphere, the normals are simply the normalized position vectors
	/// This gives us perfect normals for lighting calculations
	this->vertices.reserve(baseVertices.size());
	for (const auto& pos : baseVertices) {
		Vertex vertex;
		vertex.position = pos;
		/// For a unit sphere, position equals normal
		/// For our scaled sphere, we need to normalize the position
		vertex.normal = glm::normalize(pos);
		/// Start with white color - materials will handle actual appearance
		vertex.color = glm::vec3(1.0f);
		this->vertices.push_back(vertex);
	}

	/// Define the 20 triangular faces of the icosahedron
	/// The vertices are arranged to maintain consistent winding order
	/// This is crucial for backface culling and normal calculations
	static const std::array<std::array<uint32_t, 3>, 20> faces = {{
		/// 5 faces around vertex 0
		{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
		/// 5 adjacent faces
		{1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
		/// 5 faces around vertex 3
		{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
		/// 5 adjacent faces
		{4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
	}};

	/// Reserve space for indices
	/// Each face contributes 3 indices
	this->indices.reserve(faces.size() * 3);

	/// Add all face indices
	for (const auto& face : faces) {
		for (uint32_t index : face) {
			this->indices.push_back(index);
		}
	}

	spdlog::debug("Initialized base icosahedron with {} vertices and {} triangles",
		this->vertices.size(), this->indices.size() / 3);
}

} /// namespace lillugsi::rendering