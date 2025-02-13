#pragma once

#include "vertexdata.h"

#include <array>
#include <glm/vec3.hpp>
#include <iostream>
#include <memory> /// Include for smart pointers

namespace lillugsi::planet {
class Face {
public:
	/// Constructor with vertex indices
	explicit Face(const std::array<unsigned int, 3>& vertexIndices);

	/// Default destructor - smart pointers handle their own memory
	~Face() = default;

	/// Rule of five: Define or delete copy/move constructors and assignment operators as needed
	Face(const Face& Other) = delete;
	Face& operator=(const Face& other) = delete;
	Face(Face&& Other) noexcept = default;
	Face& operator=(Face&& other) noexcept = default;

	/// Equality operator to compare vertexIndices
	bool operator==(const Face& other) const {
		return vertexIndices == other.vertexIndices;
	}

	friend std::ostream& operator<<(std::ostream& os, const Face& face);

	void setData(float value);
	[[nodiscard]] float getData() const;

	void setNeighbor(unsigned int index, std::shared_ptr<Face> neighbor);
	void addNeighbor(const std::shared_ptr<Face>& neighbor);
	[[nodiscard]] std::shared_ptr<Face> getNeighbor(unsigned int index) const;
	void setChild(unsigned int index, std::shared_ptr<Face> child);
	void addChild(const std::shared_ptr<Face>& child);
	[[nodiscard]] std::shared_ptr<Face> getChild(unsigned int index) const;
	[[nodiscard]] std::array<std::shared_ptr<Face>, 4> getChildren() const;
	void setParent(std::weak_ptr<Face> parent);
	[[nodiscard]] std::shared_ptr<Face> getParent() const; /// Gets the parent, converting the weak pointer to a shared pointer
	void setVertexIndices(const std::array<unsigned int, 3>& indices);
	[[nodiscard]] std::array<unsigned int, 3> getVertexIndices() const;
	[[nodiscard]] glm::dvec3 getMidpoint() const { return this->midpoint; }
	void calculateMidpoint(const std::vector<glm::dvec3>& vertices);

	/// Calculate and update the face normal using vertex positions
	/// @param vertices Vector of vertex positions used for calculation
	void calculateNormal(const std::vector<std::shared_ptr<VertexData>>& vertices);

	/// Get the current face normal
	/// @return Normalized vector perpendicular to face surface
	[[nodiscard]] glm::dvec3 getNormal() const { return this->normal; }

	[[nodiscard]] bool isLeaf() const { return this->leaf; };

private:
	/// Default constructor
	Face() = default;

	/// Data
	std::array<std::shared_ptr<Face>, 4> children;
	std::array<std::shared_ptr<Face>, 3> neighbors;
	std::weak_ptr<Face> parent; /// weak_ptr for a non-owning, nullable reference to the parent
	float data{0.0f};
	std::array<unsigned int, 3> vertexIndices{{0, 0, 0}};
	bool leaf{true};
	glm::dvec3 midpoint{0.0f};
	glm::dvec3 normal{0.0, 1.0, 0.0};
};

class FaceVisitor {
public:
	virtual ~FaceVisitor() = default;
	virtual void visit(std::shared_ptr<Face> face) = 0;
};
} /// namespace lillugsi::planet