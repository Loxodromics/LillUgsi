#pragma once

#include "vector3.h"
#include "face.h"
#include <vector>
#include <map>

namespace lillugsi::planet {
class PlanetData {
public:
	PlanetData();
	~PlanetData();

	/// Methods for icosphere generation and manipulation
	void subdivide(int levels);

	/// Accessors
	[[nodiscard]] std::vector<Vector3> getVertices() const;
	[[nodiscard]] std::vector<unsigned int> getIndices() const;

	/// Visitor
	static void applyVisitorToFace(const std::shared_ptr<Face> &face, FaceVisitor& visitor);
	void applyVisitor(FaceVisitor& visitor) const;

	std::shared_ptr<Face> getFaceAtPoint(const Vector3& point) const;

private:
	/// Copy constructor
	PlanetData(const PlanetData& other);
	/// Assignment operator
	PlanetData& operator=(const PlanetData& other);

	void initializeBaseIcosahedron();

	/// Helper methods
	unsigned int addVertex(Vector3 vertex);
	std::shared_ptr<Face> addFace(unsigned int v1, unsigned int v2, unsigned int v3);

	unsigned int getOrCreateMidpointIndex(unsigned int index1, unsigned int index2); /// Helper to handle midpoint vertices
	void subdivideFace(const std::shared_ptr<Face> &face, unsigned int currentLevel, unsigned int targetLevel);

	void setNeighbors();
	void setNeighborsForBaseFaces() const;
	void setNeighborsForFace(const std::shared_ptr<Face>& face);

	std::shared_ptr<Face> getFaceAtPointRecursive(const std::shared_ptr<Face>& face,
											  const Vector3& normalizedPoint) const;

	bool intersectsLine(const std::shared_ptr<Face>& face,
							const Vector3& lineStart, const Vector3& lineEnd) const;

	/// Data
	std::vector<Vector3> vertices;
	std::vector<unsigned int> indices;
	std::map<std::pair<unsigned int, unsigned int>, unsigned int> midpointIndexCache; /// Cache to store midpoints
	std::vector<std::shared_ptr<Face>> baseFaces;

	static constexpr float EPSILON = 0.0000001f;
};
} /// namespace lillugsi::planet