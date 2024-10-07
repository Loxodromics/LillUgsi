#pragma once

#include "vulkan/vulkanexception.h"
#include <glm/glm.hpp>
#include <vector>

namespace lillugsi::rendering {

/// Vertex structure defining the format of our vertex data
/// This structure is crucial for defining how our mesh data is laid out in memory
/// and how it should be interpreted by the GPU
struct Vertex {
	glm::vec3 position; /// The 3D position of the vertex
	glm::vec3 normal;   /// The normal vector of the vertex, used for lighting calculations
	glm::vec3 color;    /// The color of the vertex
};

/// Abstract base class for all mesh types
/// This class provides a common interface for different types of meshes,
/// allowing for polymorphic behavior in our renderer
class Mesh {
public:
	Mesh() = default;
	virtual ~Mesh() = default;

	/// Pure virtual function to generate geometry
	/// Derived classes must implement this to define their specific geometry
	virtual void generateGeometry() = 0;

	/// Getter for vertex data
	/// @return A const reference to the vector of vertices
	const std::vector<Vertex>& getVertices() const { return this->vertices; }

	/// Getter for index data
	/// @return A const reference to the vector of indices
	const std::vector<uint32_t>& getIndices() const { return this->indices; }

protected:
	std::vector<Vertex> vertices;    /// Storage for vertex data
	std::vector<uint32_t> indices;   /// Storage for index data
};

} /// namespace lillugsi::rendering