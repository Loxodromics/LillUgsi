#include "planetdata.h"
#include "datasettingvisitor.h"

#include <spdlog/spdlog.h>
#include <algorithm> /// For std::min and std::max
#include <map>
#include <iostream>

namespace lillugsi::planet {
PlanetData::PlanetData() {
	this->initializeBaseIcosahedron();
}


PlanetData::~PlanetData() {
	/// Cleanup if needed
}

PlanetData::PlanetData(const PlanetData& other)
	: vertices(other.vertices)
	, indices(other.indices) {
	/// Copy constructor implementation
}

PlanetData& PlanetData::operator=(const PlanetData& other) {
	if (this != &other) {
		this->vertices = other.vertices;
		this->indices = other.indices;
	}
	return *this;
}

std::vector<glm::vec3> PlanetData::getVertices() const {
	return this->vertices;
}

std::vector<unsigned int> PlanetData::getIndices() const {
	return this->indices;
}

void PlanetData::applyVisitorToFace(const std::shared_ptr<Face> &face, FaceVisitor& visitor) {
	if (!face)
		return;
	
	visitor.visit(face); /// Apply the visitor to the current face

	/// Recursively apply the visitor to all children
	for (auto& child : face->getChildren()) {
		applyVisitorToFace(child, visitor);
	}
}

void PlanetData::applyVisitor(FaceVisitor& visitor) const {
	for (auto& baseFace : this->baseFaces) {
		PlanetData::applyVisitorToFace(baseFace, visitor);
	}
}

std::shared_ptr<Face> PlanetData::getFaceAtPoint(const glm::vec3 &point) const {
	glm::vec3 normalizedPoint = glm::normalize(point) * 2.0f;
	for (const auto& baseFace : this->baseFaces) {
		auto result = this->getFaceAtPointRecursive(baseFace, normalizedPoint);
		if (result)
			return result;
	}
	return nullptr;
}

unsigned int PlanetData::addVertex(const glm::vec3 vertex) {
	this->vertices.push_back(vertex);
	// spdlog::debug("addVertex: {}", vertices.size() - 1);
	/// Assuming the vertices vector is zero-indexed, the position of the newly added vertex
	/// is at the size of the vector before the push_back minus 1.
	return this->vertices.size() - 1;
}

std::shared_ptr<Face> PlanetData::addFace(const unsigned int v1, const unsigned int v2, const unsigned int v3) {
	spdlog::trace("addFace({}, {}, {})", v1, v2, v3);
	/// Adding indices for a triangular face
	this->indices.push_back(v3);
	this->indices.push_back(v2);
	this->indices.push_back(v1);

	/// Create and store the Face object
	std::shared_ptr<Face> face = std::make_shared<Face>(std::array<unsigned int, 3>{v3, v2, v1});
	return face;
}

void PlanetData::subdivide(int levels) {
	for (auto& baseFace : baseFaces) {
		this->subdivideFace(baseFace, 0, levels);
	}

	/// After subdivision, we run a separate function
	/// to recursively set neighbors for each face.
	this->setNeighbors();
}

unsigned int PlanetData::getOrCreateMidpointIndex(unsigned int index1, unsigned int index2) {
	/// Ensure the first index is always the smaller one to avoid duplicates
	std::pair<int, int> key(std::min(index1, index2), std::max(index1, index2));
	spdlog::trace("getOrCreateMidpointIndex({}, {})", key.first, key.second);

	/// Check if this midpoint has already been created
	if (this->midpointIndexCache.find(key) != this->midpointIndexCache.end()) {
		spdlog::trace("Found cached midpoint: {}", this->midpointIndexCache[key]);
		return this->midpointIndexCache[key];
	}

	/// Create a new midpoint vertex, then normalize it to ensure it's on the unit sphere
	glm::vec3 midpoint = (vertices[index1] + vertices[index2]) * 0.5f;
	midpoint = glm::normalize(midpoint);
	unsigned int midpointIndex = this->addVertex(midpoint);
	spdlog::trace("Created new midpoint vertex: {}", midpointIndex);

	/// Add to cache
	this->midpointIndexCache[key] = midpointIndex;

	return midpointIndex;
}

void PlanetData::initializeBaseIcosahedron() {
	this->vertices.clear();
	this->indices.clear();
	this->midpointIndexCache.clear();

	float phi = (1.0f + sqrt(5.0f)) * 0.5f; /// golden ratio
	float a = 1.0f;
	float b = 1.0f / phi;

	/// Add vertices
	this->addVertex(glm::normalize(glm::vec3(0, b, -a)));  // v0
	this->addVertex(glm::normalize(glm::vec3(b, a, 0)));   // v1
	this->addVertex(glm::normalize(glm::vec3(-b, a, 0)));  // v2
	this->addVertex(glm::normalize(glm::vec3(0, b, a)));   // v3
	this->addVertex(glm::normalize(glm::vec3(0, -b, a)));  // v4
	this->addVertex(glm::normalize(glm::vec3(-a, 0, b)));  // v5
	this->addVertex(glm::normalize(glm::vec3(0, -b, -a))); // v6
	this->addVertex(glm::normalize(glm::vec3(a, 0, -b)));  // v7
	this->addVertex(glm::normalize(glm::vec3(a, 0, b)));   // v8
	this->addVertex(glm::normalize(glm::vec3(-a, 0, -b))); // v9
	this->addVertex(glm::normalize(glm::vec3(b, -a, 0)));  // v10
	this->addVertex(glm::normalize(glm::vec3(-b, -a, 0))); // v11

	/// Add faces
	this->baseFaces.push_back(this->addFace(2, 1, 0));
	this->baseFaces.push_back(this->addFace(2, 3, 1));
	this->baseFaces.push_back(this->addFace(5, 4, 3));
	this->baseFaces.push_back(this->addFace(4, 8, 3));
	this->baseFaces.push_back(this->addFace(7, 6, 0));
	this->baseFaces.push_back(this->addFace(6, 9, 0));
	this->baseFaces.push_back(this->addFace(11, 10, 4));
	this->baseFaces.push_back(this->addFace(10, 11, 6));
	this->baseFaces.push_back(this->addFace(9, 5, 2));
	this->baseFaces.push_back(this->addFace(5, 9, 11));
	this->baseFaces.push_back(this->addFace(8, 7, 1));
	this->baseFaces.push_back(this->addFace(7, 8, 10));
	this->baseFaces.push_back(this->addFace(2, 5, 3));
	this->baseFaces.push_back(this->addFace(8, 1, 3));
	this->baseFaces.push_back(this->addFace(9, 2, 0));
	this->baseFaces.push_back(this->addFace(1, 7, 0));
	this->baseFaces.push_back(this->addFace(11, 9, 6));
	this->baseFaces.push_back(this->addFace(7, 10, 6));
	this->baseFaces.push_back(this->addFace(5, 11, 4));
	this->baseFaces.push_back(this->addFace(10, 8, 4));
}

void PlanetData::subdivideFace(const std::shared_ptr<Face> &face, unsigned int currentLevel, unsigned int targetLevel) {
	if (currentLevel >= targetLevel) {
		return; /// Base case: Reached the desired level of subdivision
	}
	spdlog::trace("subdivideFace: vertices[{}, {}, {}], level {}/{}",
		face->getVertexIndices()[0],
		face->getVertexIndices()[1],
		face->getVertexIndices()[2],
		currentLevel,
		targetLevel);

	/// Calculate midpoints and create new vertices (if necessary)
	const unsigned int mid1 = this->getOrCreateMidpointIndex(face->getVertexIndices()[0], face->getVertexIndices()[1]);
	const unsigned int mid2 = this->getOrCreateMidpointIndex(face->getVertexIndices()[1], face->getVertexIndices()[2]);
	const unsigned int mid3 = this->getOrCreateMidpointIndex(face->getVertexIndices()[2], face->getVertexIndices()[0]);

	/// Create new faces using the original vertices and the new midpoints
	std::array<std::shared_ptr<Face>, 4> newFaces = {
		this->addFace(face->getVertexIndices()[0], mid1, mid3),
		this->addFace(mid1, face->getVertexIndices()[1], mid2),
		this->addFace(mid3, mid2, face->getVertexIndices()[2]),
		this->addFace(mid1, mid2, mid3)
	};

	/// Set parent-child relationships
	for (auto& newFace : newFaces) {
		newFace->setParent(face);
		face->addChild(newFace);
	}

	/// Recursively subdivide the new faces
	for (auto& newFace : newFaces) {
		this->subdivideFace(newFace, currentLevel + 1, targetLevel);
	}
}

void PlanetData::setNeighbors() {
	this->setNeighborsForBaseFaces();

	for (const auto& baseFace : this->baseFaces) {
		for (const auto& face : baseFace->getChildren()) {
			if (face)
				this->setNeighborsForFace(face);
			else
				spdlog::trace("no child");
		}
	}
}

void PlanetData::setNeighborsForBaseFaces() const {
	for (auto& currentFace : this->baseFaces) {
		std::array<unsigned int, 3> currentIndices = currentFace->getVertexIndices();
		std::sort(currentIndices.begin(), currentIndices.end());

		int neighborCount = 0;

		for (auto& potentialNeighbor : this->baseFaces) {
			if (currentFace == potentialNeighbor) continue; /// Skip the same face

			std::array<unsigned int, 3> neighborIndices = potentialNeighbor->getVertexIndices();
			std::sort(neighborIndices.begin(), neighborIndices.end());

			std::array<unsigned int, 3> intersection{};
			const auto it = std::set_intersection(currentIndices.begin(), currentIndices.end(),
			                                      neighborIndices.begin(), neighborIndices.end(),
			                                      intersection.begin());
			const size_t matches = it - intersection.begin();

			if (matches == 2) { /// If exactly two indices match, it's a neighbor
				spdlog::trace("Setting neighbor for base face: [{}, {}, {}]",
					potentialNeighbor->getVertexIndices()[0],
					potentialNeighbor->getVertexIndices()[1],
					potentialNeighbor->getVertexIndices()[2]);
				currentFace->setNeighbor(neighborCount++, potentialNeighbor);
				if (neighborCount == 3)
					break; /// Each face has exactly 3 neighbors
			}
		}
		spdlog::info("Found {} neighbors for base face", neighborCount);
	}
}


void PlanetData::setNeighborsForFace(const std::shared_ptr<Face>& face) {
	auto parent = face->getParent();
	if (!parent) {
		spdlog::warn("setNeighborsForFace called with null parent");
		return;
	}

	spdlog::trace("Setting neighbors for face");

	int neighborCount = 0;
	std::array<unsigned int, 3> myIndices = face->getVertexIndices();
	std::sort(myIndices.begin(), myIndices.end());

	/// check my siblings first
	for (const auto& sibling : parent->getChildren()) {
		if (!sibling)
			continue; /// Skip if sibling has no child at this index

		std::array<unsigned int, 3> siblingIndices = sibling->getVertexIndices();
		std::sort(siblingIndices.begin(), siblingIndices.end());

		/// Count matching indices
		std::array<unsigned int, 3> intersection{};
		const auto it = std::set_intersection(myIndices.begin(), myIndices.end(),
		                                      siblingIndices.begin(), siblingIndices.end(),
		                                      intersection.begin());
		const size_t matches = it - intersection.begin();
		spdlog::trace("Current face indices: [{}, {}, {}]",
			myIndices[0], myIndices[1], myIndices[2]);
		spdlog::trace("Sibling indices: [{}, {}, {}]",
			siblingIndices[0], siblingIndices[1], siblingIndices[2]);
		spdlog::trace("Found {} matching indices", matches);

		/// If exactly two indices match, it's a neighbor
		if (matches == 2) {
			spdlog::trace("Setting neighbor: [{}, {}, {}]",
				sibling->getVertexIndices()[0],
				sibling->getVertexIndices()[1],
				sibling->getVertexIndices()[2]);
			neighborCount++;
			face->addNeighbor(sibling);
		}
	}

	/// Now check my cusins 
	const auto grandparent = parent->getParent();
	if (grandparent) {
		std::array<std::shared_ptr<Face>, 4> siblings = grandparent->getChildren();

		/// Check siblings of the parent (which are my siblings and cousins) to find neighbors
		for (auto& sibling : siblings) {
			if (sibling == parent || !sibling)
				continue; /// Skip my parent (did that already) and any null siblings

			for (int i = 0; i < 4; ++i) { /// Checking all 4 children
				auto siblingChild = sibling->getChild(i);
				if (!siblingChild)
					continue; /// Skip if sibling has no child at this index

				std::array<unsigned int, 3> siblingIndices = siblingChild->getVertexIndices();
				std::sort(siblingIndices.begin(), siblingIndices.end());

				/// Count matching indices
				std::array<unsigned int, 3> intersection{};
				const auto it = std::set_intersection(myIndices.begin(), myIndices.end(),
												siblingIndices.begin(), siblingIndices.end(),
												intersection.begin());
				size_t matches = it - intersection.begin();
				spdlog::trace("Current face indices: [{}, {}, {}]",
					myIndices[0], myIndices[1], myIndices[2]);
				spdlog::trace("Sibling indices: [{}, {}, {}]",
					siblingIndices[0], siblingIndices[1], siblingIndices[2]);
				spdlog::trace("Found {} matching indices", matches);

				/// If exactly two indices match, it's a neighbor
				if (matches == 2) {
					spdlog::debug("Setting neighbor: [{}, {}, {}]",
						sibling->getVertexIndices()[0],
						sibling->getVertexIndices()[1],
						sibling->getVertexIndices()[2]);
					neighborCount++;
					face->addNeighbor(siblingChild);
				}
			}
		}
	}
	spdlog::info("Found {} total neighbors", neighborCount);

	/// Recursively set neighbors for children
	for (int i = 0; i < 4; ++i) { /// Updated to iterate over all 4 children
		if (face->getChild(i))
			this->setNeighborsForFace(face->getChild(i));
		else
			spdlog::trace("No child at index {}", i);
	}
}

std::shared_ptr<Face> PlanetData::getFaceAtPointRecursive(const std::shared_ptr<Face> &face, const glm::vec3 &normalizedPoint) const {
	if (!intersectsLine(face, glm::vec3(0,0,0), normalizedPoint)) {
		return nullptr;
	}

	/// If this is a leaf face, return it
	if (face->getChildren().empty()) {
		return face;
	}

	/// Check children
	for (const auto& child : face->getChildren()) {
		auto result = this->getFaceAtPointRecursive(child, normalizedPoint);
		if (result)
			return result;
	}

	return nullptr;
}

bool PlanetData::intersectsLine(const std::shared_ptr<Face> &face, const glm::vec3 &lineStart,
	const glm::vec3 &lineEnd) const {
	/// Möller-Trumbore algorithm for intersecting line - triangle
	/// Get the vertices of the face
	std::array<unsigned int, 3> vertexIndices = face->getVertexIndices();
	const glm::vec3& v0 = vertices[vertexIndices[0]];
	const glm::vec3& v1 = vertices[vertexIndices[1]];
	const glm::vec3& v2 = vertices[vertexIndices[2]];

	glm::vec3 direction = lineEnd - lineStart;

	/// Edge vectors
	glm::vec3 e1 = v1 - v0;
	glm::vec3 e2 = v2 - v0;

	/// Calculate determinant
	glm::vec3 pvec = glm::cross(direction, e2);
	float det = glm::dot(e1, pvec);

	/// If determinant is near zero, ray lies in plane of triangle
	if (std::fabs(det) < EPSILON) return false;

	float invDet = 1.0f / det;

	/// Calculate u parameter and test bounds
	glm::vec3 tvec = lineStart - v0;
	float u = glm::dot(tvec, pvec) * invDet;
	if (u < 0.0f || u > 1.0f) return false;

	/// Prepare to test v parameter
	glm::vec3 qvec = glm::cross(tvec, e1);

	/// Calculate v parameter and test bounds
	float v = glm::dot(direction, qvec) * invDet;
	if (v < 0.0f || u + v > 1.0f) return false;

	/// Calculate t, ray intersects triangle
	float t = glm::dot(e2, qvec) * invDet;

	/// Check if the intersection point is between lineStart and lineEnd
	return (t >= 0.0f && t <= 1.0f);
}

} /// namespace lillugsi::planet