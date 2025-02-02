#pragma once

#include "face.h"
#include "vertexdata.h"
#include "vertexvisitor.h"
#include <glm/glm.hpp>
#include <vector>
#include <map>

namespace lillugsi::planet {
class PlanetData {
public:
	PlanetData();
	~PlanetData() = default;

	/// Methods for icosphere generation and manipulation
	void subdivide(int levels);

	/// Accessors
	/// Return vertices as vec3 for backwards compatibility with rendering/other systems
	[[nodiscard]] std::vector<glm::dvec3> getVertices() const;
	[[nodiscard]] std::vector<unsigned int> getIndices() const;

	/// Visitor
	static void applyVisitorToFace(const std::shared_ptr<Face> &face, FaceVisitor& visitor);
	void applyFaceVisitor(FaceVisitor& visitor) const;
	void applyVertexVisitor(VertexVisitor& visitor) const; ///< Apply a vertex visitor to all vertices in the mesh

	[[nodiscard]] std::shared_ptr<Face> getFaceAtPoint(const glm::dvec3& point) const;

	/// Get the height at a specific point on the planet surface
	/// Returns the elevation of the nearest vertex to maintain data integrity
	/// @param point Any point in space (will be normalized to unit sphere)
	/// @return The elevation at the nearest vertex, or 0.0f if no face found
	[[nodiscard]] double getHeightAt(const glm::dvec3& point) const;
	[[nodiscard]] double getHeightAtNearestVertex(const glm::dvec3& point) const;

	/// Get interpolated height at a specific point on the planet surface
	/// Uses barycentric coordinates to smoothly blend between vertex elevations
	/// @param point Any point in space (will be normalized to unit sphere)
	/// @return The interpolated elevation, or 0.0f if no face found
	[[nodiscard]] double getInterpolatedHeightAt(const glm::dvec3& point) const;

	/// Get the normal at a specific point on the planet surface
	/// Returns the normal of the nearest vertex to maintain data integrity
	/// @param point Any point in space (will be normalized to unit sphere)
	/// @return The normal at the nearest vertex, or up vector if no face found
	[[nodiscard]] glm::dvec3 getNormalAt(const glm::dvec3& point) const;
	[[nodiscard]] glm::dvec3 getNormalAtNearestVertex(const glm::dvec3& point) const;

	/// Get interpolated normal at a specific point on the planet surface
	/// Uses barycentric coordinates to smoothly blend between vertex normals
	/// The result is normalized to ensure a valid surface normal
	/// @param point Any point in space (will be normalized to unit sphere)
	/// @return The interpolated normal, or up vector if no face found
	[[nodiscard]] glm::dvec3 getInterpolatedNormalAt(const glm::dvec3& point) const;

	/// Update normals for all vertices and faces in the mesh
	/// We call this after terrain generation or any global mesh changes
	void updateNormals();

	/// Update normals for a specific vertex and its surrounding faces
	/// @param vertexIndex Index of the vertex whose normals need updating
	void updateNormalsForVertex(size_t vertexIndex);

	void verifyNormalDirections() const;

private:
	/// Copy constructor
	PlanetData(const PlanetData& other);
	/// Assignment operator
	PlanetData& operator=(const PlanetData& other);

	void initializeBaseIcosahedron();

	/// Set up neighbor relationships for initial icosahedron vertices
	void setupInitialVertexNeighbors();

	/// Rebuilds all vertex neighbor relationships using face data
	/// Must be called after subdivision is complete
	void rebuildAllVertexNeighbors();

	/// Helper methods
	/// Returns index of the new vertex in vertices vector
	unsigned int addVertex(const glm::dvec3& position);
	std::shared_ptr<Face> addFace(unsigned int v1, unsigned int v2, unsigned int v3);

	unsigned int getOrCreateMidpointIndex(unsigned int index1, unsigned int index2);
	void subdivideFace(const std::shared_ptr<Face> &face, unsigned int currentLevel, unsigned int targetLevel);

	void setNeighbors();
	void setNeighborsForBaseFaces() const;
	void setNeighborsForFace(const std::shared_ptr<Face>& face);

	[[nodiscard]] std::shared_ptr<Face> getFaceAtPointRecursive(const std::shared_ptr<Face>& face,
															   const glm::dvec3& normalizedPoint) const;

	[[nodiscard]] bool intersectsLine(const std::shared_ptr<Face>& face,
									  const glm::dvec3& lineStart, const glm::dvec3& lineEnd) const;

	/// Calculate barycentric coordinates for a point within a face
	/// @param face The face containing the point
	/// @param point The point to calculate coordinates for (must be normalized)
	/// @return Barycentric coordinates (u,v,w) for the point
	[[nodiscard]] glm::dvec3 calculateBarycentricCoords(
		const std::shared_ptr<Face>& face,
		const glm::dvec3& point) const;

	/// Get all faces that contain a specific vertex
	/// @param vertexIndex Index of the vertex to find faces for
	/// @return Vector of faces that share the specified vertex
	[[nodiscard]] std::vector<std::shared_ptr<Face>> getFacesForVertex(size_t vertexIndex) const;

	/// Data
	/// Store VertexData objects instead of just positions
	std::vector<std::shared_ptr<VertexData>> vertices;
	std::vector<unsigned int> indices;
	std::map<std::pair<unsigned int, unsigned int>, unsigned int> midpointIndexCache;
	std::vector<std::shared_ptr<Face>> baseFaces;

	static constexpr double EPSILON = 0.0000001;
};
} /// namespace lillugsi::planet