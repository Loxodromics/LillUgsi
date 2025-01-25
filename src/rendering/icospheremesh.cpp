#include "icospheremesh.h"
#include "vulkan/vulkanexception.h"
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
	/// Create base icosahedron
	this->initializeBaseIcosahedron();

	/// Perform requested number of subdivisions
	for (uint32_t i = 0; i < this->subdivisions; ++i) {
		spdlog::debug("Performing subdivision {}/{}", i + 1, this->subdivisions);
		this->subdivide();
	}
}

void IcosphereMesh::applyVertexTransforms(const std::vector<VertexTransform> &transforms)
{
	/// Verify transform count matches vertex count
	/// This ensures we have exactly one transform per vertex
	if (transforms.size() != this->vertices.size()) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Transform count must match vertex count",
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	/// Apply each transform to its corresponding vertex
	/// We update all vertex properties to maintain consistency
	for (size_t i = 0; i < transforms.size(); ++i) {
		const auto &transform = transforms[i];
		auto &vertex = this->vertices[i];

		vertex.position = transform.position;
		vertex.normal = transform.normal;
		vertex.color = transform.color;
	}

	spdlog::debug("Applied {} vertex transforms to icosphere", transforms.size());
}

std::vector<glm::vec3> IcosphereMesh::getVertexPositions() const {
	/// Extract positions from vertices for external use
	/// This allows transform calculations without exposing internal vertex format
	std::vector<glm::vec3> positions;
	positions.reserve(this->vertices.size());

	for (const auto& vertex : this->vertices) {
		positions.push_back(vertex.position);
	}

	return positions;
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
		{0, 5, 11}, {0, 1, 5}, {0, 7, 1}, {0, 10, 7}, {0, 11, 10},
		/// 5 adjacent faces
		{1, 9, 5}, {5, 4, 11}, {11, 2, 10}, {10, 6, 7}, {7, 8, 1},
		/// 5 faces around vertex 3
		{3, 4, 9}, {3, 2, 4}, {3, 6, 2}, {3, 8, 6}, {3, 9, 8},
		/// 5 adjacent faces
		{4, 5, 9}, {2, 11, 4}, {6, 10, 2}, {8, 7, 6}, {9, 1, 8}
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

int64_t IcosphereMesh::generateEdgeKey(uint32_t index1, uint32_t index2) {
	/// Always use the smaller index first for consistent key generation
	/// This ensures (1,2) and (2,1) generate the same key
	const uint32_t i1 = std::min(index1, index2);
	const uint32_t i2 = std::max(index1, index2);

	/// Combine indices into a single 64-bit key
	/// We use 32 bits for each index, allowing for up to 4 billion vertices
	/// The shift by 32 puts the first index in the upper 32 bits
	return (static_cast<int64_t>(i1) << 32) | static_cast<int64_t>(i2);
}

uint32_t IcosphereMesh::getOrCreateMidpoint(uint32_t index1, uint32_t index2) {
	/// Generate a unique key for this edge
	const int64_t key = generateEdgeKey(index1, index2);

	/// Check if we already have a vertex for this edge
	auto it = this->midpointCache.find(key);
	if (it != this->midpointCache.end()) {
		return it->second;
	}

	/// No existing midpoint, so we need to create a new vertex
	/// Get the positions of the two vertices we're interpolating between
	const glm::vec3& p1 = this->vertices[index1].position;
	const glm::vec3& p2 = this->vertices[index2].position;

	/// Create the new vertex at the midpoint
	Vertex newVertex;
	/// Calculate midpoint and project onto sphere surface
	/// We normalize to ensure the vertex lies exactly on the sphere
	newVertex.position = glm::normalize(p1 + p2) * this->radius;

	/// Average and normalize the normals of the two vertices
	/// This creates smooth shading across the sphere
	newVertex.normal = glm::normalize(
		this->vertices[index1].normal +
		this->vertices[index2].normal
	);

	/// Interpolate colors for smooth color transitions
	newVertex.color = (
		this->vertices[index1].color +
		this->vertices[index2].color
	) * 0.5f;

	/// Add the new vertex to our mesh
	const uint32_t newIndex = static_cast<uint32_t>(this->vertices.size());
	this->vertices.push_back(newVertex);

	/// Cache the midpoint for future lookups
	this->midpointCache[key] = newIndex;

	return newIndex;
}

void IcosphereMesh::subdivide() {
	/// Each triangle is split into four new triangles:
	///   v1        Original triangle: v1, v2, v3
	///  /  \       After subdivision:
	/// a -- b      - v1, a, b
	///  \ /  \     - a, v2, c
	///   c -- v2   - b, c, v3
	///    \  /     - a, c, b
	///     v3      where a, b, c are new vertices

	/// Store original indices
	/// We can't modify the index buffer while iterating through it
	const std::vector<uint32_t> oldIndices = this->indices;

	/// Clear index buffer for new triangles
	/// Each triangle will become four new triangles
	this->indices.clear();
	this->indices.reserve(oldIndices.size() * 4);

	/// Process each triangle
	for (size_t i = 0; i < oldIndices.size(); i += 3) {
		/// Get indices of original triangle vertices
		const uint32_t v1 = oldIndices[i];
		const uint32_t v2 = oldIndices[i + 1];
		const uint32_t v3 = oldIndices[i + 2];

		/// Get or create vertices at the midpoints of each edge
		const uint32_t a = this->getOrCreateMidpoint(v1, v2);
		const uint32_t b = this->getOrCreateMidpoint(v1, v3);
		const uint32_t c = this->getOrCreateMidpoint(v2, v3);

		/// Add four new triangles
		/// Order is important to maintain consistent winding
		this->indices.push_back(v1); this->indices.push_back(a); this->indices.push_back(b);
		this->indices.push_back(a); this->indices.push_back(v2); this->indices.push_back(c);
		this->indices.push_back(b); this->indices.push_back(c); this->indices.push_back(v3);
		this->indices.push_back(a); this->indices.push_back(c); this->indices.push_back(b);
	}

	/// Clear midpoint cache after subdivision
	/// This prevents memory growth when subdividing multiple times
	this->midpointCache.clear();

	spdlog::debug("Subdivision complete: {} vertices, {} triangles",
		this->vertices.size(), this->indices.size() / 3);
}

} /// namespace lillugsi::rendering