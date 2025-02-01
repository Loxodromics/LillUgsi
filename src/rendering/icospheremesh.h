#pragma once

#include "mesh.h"
#include <map>
#include <cstdint>

namespace lillugsi::rendering {

/// IcosphereMesh generates a sphere by subdividing a base icosahedron
/// An icosphere provides a more uniform triangulation than UV spheres,
/// which is particularly important for:
/// - Even distribution of vertices (better for physics)
/// - More consistent lighting and shading
/// - Efficient memory use (no vertex clustering at poles)
class IcosphereMesh : public Mesh {
public:
	/// Vertex transform data for mesh updates
	/// This structure contains all data needed to update a vertex's properties
	/// We use this interface to decouple mesh updates from specific data sources
	struct VertexTransform {
		glm::vec3 oldPosition;
		glm::vec3 position;  /// New vertex position
		glm::vec3 normal;    /// New vertex normal
		glm::vec3 color;     /// New vertex color
	};

	/// Create an icosphere with given parameters
	/// @param radius Controls the size of the sphere
	/// @param subdivisions Number of times to subdivide the base icosahedron
	/// More subdivisions create a smoother sphere but increase geometry complexity:
	/// - 0: 12 vertices, 20 triangles (base icosahedron)
	/// - 1: 42 vertices, 80 triangles
	/// - 2: 162 vertices, 320 triangles
	/// - 3: 642 vertices, 1280 triangles
	/// - 4: 2562 vertices, 5120 triangles
	explicit IcosphereMesh(float radius = 1.0f, uint32_t subdivisions = 1);
	~IcosphereMesh() override = default;

	void generateGeometry() override;

	/// Apply transforms to update mesh vertices
	/// @param transforms Vector of transforms matching vertex count
	/// @throws VulkanException if transform count doesn't match vertex count
	void applyVertexTransforms(const std::vector<VertexTransform>& transforms);

	/// Get current vertex positions
	/// Useful for computing transforms based on current mesh state
	/// @return Vector of current vertex positions
	[[nodiscard]] std::vector<glm::vec3> getVertexPositions() const;

	/// Get the current radius of the icosphere
	/// @return The sphere's base radius
	[[nodiscard]] float getRadius() const { return this->radius; }

	/// Get the subdivision level
	/// @return Number of subdivision steps applied
	[[nodiscard]] uint32_t getSubdivisions() const { return this->subdivisions; }

private:
	/// Initialize the base icosahedron geometry
	/// We start with a regular icosahedron because:
	/// - It's the most complex Platonic solid
	/// - Its faces are evenly distributed
	/// - It provides a good base for spherical approximation
	void initializeBaseIcosahedron();

	/// Subdivide the current geometry once
	/// Each triangle is split into four smaller triangles while maintaining:
	/// - Vertex sharing between triangles
	/// - Proper face orientation
	/// - Even distribution of vertices
	void subdivide();

	/// Get or create a vertex at the midpoint between two existing vertices
	/// We cache midpoints to avoid duplicate vertices, which:
	/// - Reduces memory usage
	/// - Ensures proper vertex sharing for smooth shading
	/// - Maintains mesh integrity during subdivision
	/// @param index1 Index of first vertex
	/// @param index2 Index of second vertex
	/// @return Index of the midpoint vertex (either existing or newly created)
	[[nodiscard]] uint32_t getOrCreateMidpoint(uint32_t index1, uint32_t index2);

	/// Generate a unique key for edge midpoint caching
	/// We need this to efficiently look up existing midpoints
	/// @param index1 First vertex index
	/// @param index2 Second vertex index
	/// @return Unique key for this edge
	[[nodiscard]] static int64_t generateEdgeKey(uint32_t index1, uint32_t index2);

	void debugVertexOutliers();

	/// Cache for midpoint vertices during subdivision
	/// The key is a 64-bit hash of the two vertex indices
	/// The value is the index of the midpoint vertex
	/// We use this to avoid creating duplicate vertices
	std::map<int64_t, uint32_t> midpointCache;

	float radius;                    /// Base radius of the sphere
	uint32_t subdivisions;          /// Number of subdivision steps
};

} /// namespace lillugsi::rendering