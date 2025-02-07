#include "planetdata.h"
#include "datasettingvisitor.h"

#include <spdlog/spdlog.h>
#include <algorithm> /// For std::min and std::max
#include <map>
#include <glm/ext/quaternion_geometric.hpp>
#include <iostream>
#include <set>

namespace lillugsi::planet {
PlanetData::PlanetData() {
	this->initializeBaseIcosahedron();
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

std::vector<glm::dvec3> PlanetData::getVertices() const {
	std::vector<glm::dvec3> positions;
	positions.reserve(this->vertices.size());

	/// Extract positions from VertexData for compatibility
	for (const auto& vertex : this->vertices) {
		positions.push_back(vertex->getPosition());
	}
	return positions;
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

void PlanetData::applyFaceVisitor(FaceVisitor& visitor) const {
	for (auto& baseFace : this->baseFaces) {
		PlanetData::applyVisitorToFace(baseFace, visitor);
	}
}

void PlanetData::applyVertexVisitor(VertexVisitor& visitor) const {
	/// Process all vertices in order
	/// Order might matter for some algorithms, so we maintain the same
	/// traversal order for consistency
	for (auto& vertex : this->vertices) {
		visitor.visit(vertex);
	}
}

std::shared_ptr<Face> PlanetData::getFaceAtPoint(const glm::dvec3 &point) const {
	glm::dvec3 normalizedPoint = glm::normalize(point) * 2.0;

	for (const auto& baseFace : this->baseFaces) {
		// Check if face is pointing roughly in the same direction as our point
		// We use a dot product threshold slightly less than 0 to account for faces
		// that might be partially visible from this direction
		if (glm::dot(glm::normalize(baseFace->getMidpoint()), glm::normalize(normalizedPoint)) > -0.2f) {
			auto result = this->getFaceAtPointRecursive(baseFace, normalizedPoint);
			if (result)
				return result;
		}
		else {
		}
	}
	spdlog::warn("baseFaces test failed");
	return nullptr;
}

double PlanetData::getHeightAt(const glm::dvec3& point) const {
	/// First find which face contains this point
	auto face = this->getFaceAtPoint(point);
	if (!face) {
		spdlog::warn("getHeightAt: No face found for point ({}, {}, {})",
			point.x, point.y, point.z);
		return this->getHeightAtNearestVertex(point);
	}

	/// Get vertex indices for this face
	const auto indices = face->getVertexIndices();

	/// Find the closest vertex to our query point
	double minDistance = std::numeric_limits<double>::max();
	double nearestElevation = 0.0f;

	/// We check each vertex of the face to find the nearest one
	/// This ensures we're using actual data points rather than
	/// potentially invalid interpolated values
	for (unsigned int index : indices) {
		const auto& vertex = this->vertices[index];
		double distance = glm::length(vertex->getPosition() - point);

		if (distance < minDistance) {
			minDistance = distance;
			nearestElevation = vertex->getElevation();
		}
	}

	spdlog::trace("Found height {} at point ({}, {}, {})",
		nearestElevation, point.x, point.y, point.z);
	return nearestElevation;
}

double PlanetData::getHeightAtNearestVertex(const glm::dvec3& point) const {
	double minDistance = std::numeric_limits<double>::max();
	double nearestElevation = 0.0f;

	for (const auto& vertex : this->vertices) {
		double distance = glm::length(vertex->getPosition() - point);

		if (distance < minDistance) {
			minDistance = distance;
			nearestElevation = vertex->getElevation();
		}
	}

	spdlog::trace("Found height {} at point ({}, {}, {})",
		nearestElevation, point.x, point.y, point.z);
	return nearestElevation;
}

double PlanetData::getInterpolatedHeightAt(const glm::dvec3& point) const {
	/// Find containing face as before
	auto face = this->getFaceAtPoint(point);
	if (!face) {
		spdlog::warn("getInterpolatedHeightAt: No face found for point ({}, {}, {})",
			point.x, point.y, point.z);
		return 0.0f;
	}

	/// Calculate barycentric coordinates for interpolation
	/// These tell us how much each vertex contributes to our point
	const glm::dvec3 baryCoords = this->calculateBarycentricCoords(face, point);

	/// Get vertex indices and their elevations
	const auto indices = face->getVertexIndices();
	const double elevations[3] = {
		this->vertices[indices[0]]->getElevation(),
		this->vertices[indices[1]]->getElevation(),
		this->vertices[indices[2]]->getElevation()
	};

	/// Blend elevations using barycentric coordinates
	/// This gives us a smooth interpolation between the vertices
	const double interpolatedHeight =
		elevations[0] * baryCoords.x +
		elevations[1] * baryCoords.y +
		elevations[2] * baryCoords.z;

	spdlog::trace("Interpolated height {} at point ({}, {}, {})",
		interpolatedHeight, point.x, point.y, point.z);
	return interpolatedHeight;
}

glm::dvec3 PlanetData::getNormalAt(const glm::dvec3& point) const {
	/// First find which face contains this point
	/// We reuse the same face finding logic as height calculations
	auto face = this->getFaceAtPoint(point);
	if (!face) {
		spdlog::warn("No face found for normal query at point ({}, {}, {}), using fallback",
			point.x, point.y, point.z);
		return this->getNormalAtNearestVertex(point); /// Return point as fallback
	}

	/// Get vertex indices for this face
	const auto indices = face->getVertexIndices();

	/// Find the closest vertex to our query point
	double minDistance = std::numeric_limits<double>::max();
	glm::dvec3 nearestNormal(0.0f, 1.0f, 0.0f);  /// Default to up vector

	/// We check each vertex of the face to find the nearest one
	/// This ensures we're using actual data points rather than
	/// potentially invalid interpolated values
	for (unsigned int index : indices) {
		const auto& vertex = this->vertices[index];
		double distance = glm::length(vertex->getPosition() - point);

		if (distance < minDistance) {
			minDistance = distance;
			nearestNormal = vertex->getNormal();
		}
	}

	spdlog::trace("Found normal ({}, {}, {}) at point ({}, {}, {})",
		nearestNormal.x, nearestNormal.y, nearestNormal.z,
		point.x, point.y, point.z);
	return nearestNormal;
}

void PlanetData::verifyNormalDirections() const {
	for (const auto& vertex : this->vertices) {
		const glm::dvec3 normalizedPos = glm::normalize(vertex->getPosition());
		const double dotProduct = glm::dot(vertex->getNormal(), normalizedPos);

		if (dotProduct < 0.0) {
			spdlog::warn("Inward-facing normal detected at vertex {}", vertex->getIndex());
			spdlog::warn("Position: ({}, {}, {})",
				vertex->getPosition().x,
				vertex->getPosition().y,
				vertex->getPosition().z);
			spdlog::warn("Normal: ({}, {}, {})",
				vertex->getNormal().x,
				vertex->getNormal().y,
				vertex->getNormal().z);
			spdlog::warn("Dot product with position: {}", dotProduct);
		}
	}
}

glm::dvec3 PlanetData::getNormalAtNearestVertex(const glm::dvec3& point) const {
	double minDistance = std::numeric_limits<double>::max();
	glm::dvec3 nearestNormal(0.0f, 1.0f, 0.0f);  /// Default to up vector

	for (const auto& vertex : this->vertices) {
		double distance = glm::length(vertex->getPosition() - point);

		if (distance < minDistance) {
			minDistance = distance;
			nearestNormal = vertex->getNormal();
		}
	}

	spdlog::trace("Found height ({}, {}, {}) at point ({}, {}, {})",
		nearestNormal.x, nearestNormal.y, nearestNormal.z, point.x, point.y, point.z);
	return nearestNormal;
}

glm::dvec3 PlanetData::getInterpolatedNormalAt(const glm::dvec3 &point) const {
	/// Find containing face as before
	auto face = this->getFaceAtPoint(point);
	if (!face) {
		spdlog::warn(
			"No face found for normal interpolation at point ({}, {}, {})",
			point.x,
			point.y,
			point.z);
		return this->getNormalAtNearestVertex(point); /// Return postion vector as fallback
	}

	/// Calculate barycentric coordinates for interpolation
	/// These tell us how much each vertex contributes to our point
	const glm::dvec3 baryCoords = this->calculateBarycentricCoords(face, point);

	/// Get vertex indices and their normals
	const auto indices = face->getVertexIndices();
	const std::array<glm::dvec3, 3> normals
		= {this->vertices[indices[0]]->getNormal(),
		   this->vertices[indices[1]]->getNormal(),
		   this->vertices[indices[2]]->getNormal()};

	/// Blend normals using barycentric coordinates
	/// Unlike height interpolation, we need to normalize the result
	/// as a linear interpolation of normalized vectors isn't normalized
	const glm::dvec3 interpolatedNormal = normals[0] * baryCoords.x + normals[1] * baryCoords.y
										  + normals[2] * baryCoords.z;

	/// Ensure we return a normalized vector
	/// We check length to avoid division by zero in case of degenerate geometry
	const double length = glm::length(interpolatedNormal);
	if (length > EPSILON) {
		return interpolatedNormal / length;
	}
	else {
		spdlog::warn("Generated zero-length interpolated normal, falling back to up vector");
		return normalize(point);
	}
}

void PlanetData::updateNormals() {
	for (const auto& face : this->faces) {
		if (face->isLeaf()) {
			face->calculateNormal(this->vertices);
		}
	}

	/// Then update vertex normals
	for (size_t i = 0; i < this->vertices.size(); ++i) {
		this->updateNormalsForVertex(i);
	}

	// this->verifyNormalDirections();
}

void PlanetData::updateNormalsForVertex(size_t vertexIndex) {
	auto vertex = this->vertices[vertexIndex];
	if (!vertex)
		return;

	/// Get all faces that contain this vertex
	const auto faces = this->getFacesForVertex(vertexIndex);

	/// Calculate and set the new normal
	const glm::dvec3 newNormal = vertex->calculateNormalFromFaces(faces, this->vertices);
	vertex->setNormal(newNormal);
}

unsigned int PlanetData::addVertex(const glm::dvec3& position) {
	/// Create new VertexData object with given position and current index
	auto vertex = std::make_shared<VertexData>(position, this->vertices.size());
	this->vertices.push_back(vertex);

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
	face->calculateMidpoint(this->getVertices());
	this->faces.push_back(face);

	return face;
}

void PlanetData::subdivide(int levels) {
	/// First perform all geometric subdivision
	for (auto& baseFace : this->baseFaces) {
		this->subdivideFace(baseFace, 0, levels);
	}

	/// Now rebuild all neighbor relationships for the final mesh
	this->rebuildAllVertexNeighbors();

	/// After neighbor relationships are established, we can set up face neighbors
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

	/// Calculate midpoint between two vertices, then normalize it to ensure it's on the unit sphere
	const glm::dvec3 pos1 = this->vertices[index1]->getPosition();
	const glm::dvec3 pos2 = this->vertices[index2]->getPosition();
	glm::dvec3 midpoint = (pos1 + pos2) * 0.5;
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

	double phi = (1.0f + sqrt(5.0f)) * 0.5f; /// golden ratio
	double a = 1.0f;
	double b = 1.0f / phi;

	/// Add vertices
	this->addVertex(glm::normalize(glm::dvec3(0, b, -a)));  // v0
	this->addVertex(glm::normalize(glm::dvec3(b, a, 0)));   // v1
	this->addVertex(glm::normalize(glm::dvec3(-b, a, 0)));  // v2
	this->addVertex(glm::normalize(glm::dvec3(0, b, a)));   // v3
	this->addVertex(glm::normalize(glm::dvec3(0, -b, a)));  // v4
	this->addVertex(glm::normalize(glm::dvec3(-a, 0, b)));  // v5
	this->addVertex(glm::normalize(glm::dvec3(0, -b, -a))); // v6
	this->addVertex(glm::normalize(glm::dvec3(a, 0, -b)));  // v7
	this->addVertex(glm::normalize(glm::dvec3(a, 0, b)));   // v8
	this->addVertex(glm::normalize(glm::dvec3(-a, 0, -b))); // v9
	this->addVertex(glm::normalize(glm::dvec3(b, -a, 0)));  // v10
	this->addVertex(glm::normalize(glm::dvec3(-b, -a, 0))); // v11

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

	/// Set up initial neighbor relationships
	this->setupInitialVertexNeighbors();
}

void PlanetData::setupInitialVertexNeighbors() {
	/// Must be called after vertices and faces are created but before subdivision
	/// For each face in the initial icosahedron, establish neighbor relationships
	/// between its vertices. Each original vertex will end up with exactly 5 neighbors.
	// First pass: Build a map of vertex connections
	std::vector<std::set<unsigned int>> vertexConnections(12); /// 12 vertices in icosahedron

	for (const auto& face : this->baseFaces) {
		const auto indices = face->getVertexIndices();

		/// Add all connections for this face
		/// For each vertex in the face, connect it to the other two vertices
		/// We use all combinations since neighbor relationships are bidirectional
		for (int i = 0; i < 3; i++) {
			unsigned int v1 = indices[i];
			unsigned int v2 = indices[(i + 1) % 3];
			vertexConnections[v1].insert(v2);
			vertexConnections[v2].insert(v1);
		}
	}

	/// Second pass: Create neighbor relationships
	for (size_t i = 0; i < vertexConnections.size(); i++) {
		for (unsigned int neighborIdx : vertexConnections[i]) {
			this->vertices[i]->addNeighbor(this->vertices[neighborIdx]);
		}
	}

	/// Verify each original vertex has exactly 5 neighbors
	for (size_t i = 0; i < 12; ++i) {  /// First 12 vertices are from original icosahedron
		const auto neighbors = this->vertices[i]->getNeighbors();
		if (neighbors.size() != 5) {
			spdlog::warn("Initial vertex {} has {} neighbors instead of expected 5",
				i, neighbors.size());
		}
	}
}

void PlanetData::rebuildAllVertexNeighbors() {
	/// Clear existing relationships
	for (const auto & vertex : this->vertices) {
		vertex->clearNeighbors();
	}

	/// Create a temporary map to store vertex connections
	std::vector<std::set<unsigned int>> vertexConnections(this->vertices.size());

	/// Process all leaf faces to build the final mesh topology
	std::function<void(const std::shared_ptr<Face>&)> processLeafFaces;
	processLeafFaces = [&processLeafFaces, &vertexConnections](const std::shared_ptr<Face>& face) {
		if (face->isLeaf()) {
			/// For each edge in the face, connect its vertices
			const auto indices = face->getVertexIndices();
			for (int i = 0; i < 3; i++) {
				const unsigned int v1 = indices[i];
				const unsigned int v2 = indices[(i + 1) % 3];
				vertexConnections[v1].insert(v2);
				vertexConnections[v2].insert(v1);
			}
		} else {
			/// Recursively process children until we reach leaf faces
			for (const auto& child : face->getChildren()) {
				if (child) {
					processLeafFaces(child);
				}
			}
		}
	};

	/// Process all base faces and their descendants
	for (const auto& baseFace : this->baseFaces) {
		processLeafFaces(baseFace);
	}

	/// Create the actual vertex relationships
	for (size_t i = 0; i < vertexConnections.size(); i++) {
		for (const unsigned int neighborIdx : vertexConnections[i]) {
			this->vertices[i]->addNeighbor(this->vertices[neighborIdx]);
		}
	}

	/// Verify topology
	for (size_t i = 0; i < this->vertices.size(); ++i) {
		const auto neighbors = this->vertices[i]->getNeighbors();
		const bool isOriginalVertex = i < 12;
		const size_t expectedNeighbors = isOriginalVertex ? 5 : 6;

		if (neighbors.size() != expectedNeighbors) {
			spdlog::warn("Vertex {} has {} neighbors, expected {}",
				i, neighbors.size(), expectedNeighbors);
		}
	}
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
	const auto parent = face->getParent();
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
	spdlog::trace("Found {} total neighbors", neighborCount);

	/// Recursively set neighbors for children
	for (int i = 0; i < 4; ++i) { /// Updated to iterate over all 4 children
		if (face->getChild(i))
			this->setNeighborsForFace(face->getChild(i));
		else
			spdlog::trace("No child at index {}", i);
	}
}

std::shared_ptr<Face> PlanetData::getFaceAtPointRecursive(const std::shared_ptr<Face> &face, const glm::dvec3 &normalizedPoint) const {
	/// Bend the test point slightly toward face midpoint
	glm::dvec3 bentPoint = glm::mix(glm::normalize(normalizedPoint), face->getMidpoint(), 0.01) * 2.0;

	if (!intersectsLine(face, glm::dvec3(0,0,0), bentPoint)) {
		return nullptr;
	}

	/// If this is a leaf face, return it
	if (face->isLeaf()) {
		return face;
	}

	/// Check children
	for (const auto& child : face->getChildren()) {
		if (child) {
			auto result = this->getFaceAtPointRecursive(child, bentPoint);
			if (result)
				return result;
		}
	}

	/// This should not really happen, if this face interects and is no leaf, then one of the
	/// children should intersect. But maybe due to limited float point precision this might happen
	spdlog::warn("Face intersected but none of its children");
	return nullptr;
}

bool PlanetData::intersectsLine(const std::shared_ptr<Face> &face, const glm::dvec3 &lineStart,
	const glm::dvec3 &lineEnd) const {

	/// Möller-Trumbore algorithm for intersecting line - triangle
	/// Get the vertices of the face
	const std::array<unsigned int, 3> vertexIndices = face->getVertexIndices();
	const glm::dvec3& v0 = vertices[vertexIndices[0]]->getPosition();
	const glm::dvec3& v1 = vertices[vertexIndices[1]]->getPosition();
	const glm::dvec3& v2 = vertices[vertexIndices[2]]->getPosition();

	const glm::dvec3 direction = lineEnd - lineStart;

	/// Edge vectors
	const glm::dvec3 e1 = v1 - v0;
	const glm::dvec3 e2 = v2 - v0;

	/// Calculate determinant
	const glm::dvec3 pvec = glm::cross(direction, e2);
	const double det = glm::dot(e1, pvec);

	/// If determinant is near zero, ray lies in plane of triangle
	if (std::fabs(det) < EPSILON)
		return false;

	const double invDet = 1.0f / det;

	/// Calculate u parameter and test bounds
	const glm::dvec3 tvec = lineStart - v0;
	const double u = glm::dot(tvec, pvec) * invDet;
	if (u < 0.0f || u > 1.0f)
		return false;

	/// Prepare to test v parameter
	const glm::dvec3 qvec = glm::cross(tvec, e1);

	/// Calculate v parameter and test bounds
	const double v = glm::dot(direction, qvec) * invDet;
	if (v < 0.0f || u + v > 1.0f)
		return false;

	/// Calculate t, ray intersects triangle
	const double t = glm::dot(e2, qvec) * invDet;

	/// Check if the intersection point is between lineStart and lineEnd
	return (t >= 0.0f && t <= 1.0f);
}

glm::dvec3 PlanetData::calculateBarycentricCoords(
	const std::shared_ptr<Face> &face, const glm::dvec3 &point) const {
	/// Get the vertices of the face
	const auto indices = face->getVertexIndices();
	const glm::dvec3 &a = this->vertices[indices[0]]->getPosition();
	const glm::dvec3 &b = this->vertices[indices[1]]->getPosition();
	const glm::dvec3 &c = this->vertices[indices[2]]->getPosition();

	/// Calculate vectors from point to vertices
	const glm::dvec3 v0 = b - a;
	const glm::dvec3 v1 = c - a;
	const glm::dvec3 v2 = point - a;

	/// Calculate dot products for barycentric computation
	const double d00 = glm::dot(v0, v0);
	const double d01 = glm::dot(v0, v1);
	const double d11 = glm::dot(v1, v1);
	const double d20 = glm::dot(v2, v0);
	const double d21 = glm::dot(v2, v1);

	/// Calculate barycentric coordinates using Cramer's rule
	const double denom = d00 * d11 - d01 * d01;
	const double v = (d11 * d20 - d01 * d21) / denom;
	const double w = (d00 * d21 - d01 * d20) / denom;
	const double u = 1.0f - v - w;

	return glm::dvec3(u, v, w);
}

std::vector<std::shared_ptr<Face>> PlanetData::getFacesForVertex(size_t vertexIndex) const {
	std::vector<std::shared_ptr<Face>> faces;

	/// Helper function to process a face and its children recursively
	std::function<void(const std::shared_ptr<Face>&)> processFace;
	processFace = [&faces, vertexIndex, &processFace](const std::shared_ptr<Face>& face) {
		if (!face) return;

		/// Check if this face contains our vertex
		const auto& indices = face->getVertexIndices();
		if (std::find(indices.begin(), indices.end(), vertexIndex) != indices.end()) {
			if (face->isLeaf()) {
				faces.push_back(face);
			}
		}

		/// Process children recursively
		for (const auto& child : face->getChildren()) {
			processFace(child);
		}
	};

	/// Process all base faces
	for (const auto& face : this->baseFaces) {
		processFace(face);
	}

	return faces;
}

} /// namespace lillugsi::planet