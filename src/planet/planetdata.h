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
	[[nodiscard]] std::vector<glm::vec3> getVertices() const;
	[[nodiscard]] std::vector<unsigned int> getIndices() const;

	/// Visitor
	static void applyVisitorToFace(const std::shared_ptr<Face> &face, FaceVisitor& visitor);
	void applyFaceVisitor(FaceVisitor& visitor) const;
	void applyVertexVisitor(VertexVisitor& visitor) const; ///< Apply a vertex visitor to all vertices in the mesh

	[[nodiscard]] std::shared_ptr<Face> getFaceAtPoint(const glm::vec3& point) const;

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
	unsigned int addVertex(const glm::vec3& position);
	std::shared_ptr<Face> addFace(unsigned int v1, unsigned int v2, unsigned int v3);

	unsigned int getOrCreateMidpointIndex(unsigned int index1, unsigned int index2);
	void subdivideFace(const std::shared_ptr<Face> &face, unsigned int currentLevel, unsigned int targetLevel);

	void setNeighbors();
	void setNeighborsForBaseFaces() const;
	void setNeighborsForFace(const std::shared_ptr<Face>& face);

	[[nodiscard]] std::shared_ptr<Face> getFaceAtPointRecursive(const std::shared_ptr<Face>& face,
															   const glm::vec3& normalizedPoint) const;

	[[nodiscard]] bool intersectsLine(const std::shared_ptr<Face>& face,
									  const glm::vec3& lineStart, const glm::vec3& lineEnd) const;

	/// Data
	/// Store VertexData objects instead of just positions
	std::vector<std::shared_ptr<VertexData>> vertices;
	std::vector<unsigned int> indices;
	std::map<std::pair<unsigned int, unsigned int>, unsigned int> midpointIndexCache;
	std::vector<std::shared_ptr<Face>> baseFaces;

	static constexpr float EPSILON = 0.0000001f;
};
} /// namespace lillugsi::planet