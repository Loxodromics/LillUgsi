#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>

namespace lillugsi::rendering {

/// Vertex structure defining the format of vertex data
/// This structure is crucial for defining how mesh data is laid out in memory
/// and how it should be interpreted by the GPU
struct Vertex {
	/// The 3D position of the vertex
	glm::vec3 position;

	/// The normal vector of the vertex, used for lighting calculations
	glm::vec3 normal;

	/// The tangent vector of the vertex, used for normal mapping
	/// Tangents define the direction of the positive X axis in tangent space
	/// Together with the normal and bitangent, this forms the TBN matrix
	/// which transforms normals from tangent space to world space
	glm::vec3 tangent;

	/// The color of the vertex
	glm::vec3 color;

	/// Texture coordinates (UV) for mapping textures onto the surface
	/// These coordinates determine how textures are projected onto geometry:
	/// - U: Horizontal coordinate, ranges from 0 (left) to 1 (right)
	/// - V: Vertical coordinate, ranges from 0 (bottom) to 1 (top)
	/// Using vec2 for efficiency as we only need two components
	glm::vec2 texCoord;

	/// Get the binding description for this vertex format
	/// This describes how to interpret vertex data in the vertex buffer
	/// @return The vertex binding description
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;  /// We use a single vertex buffer
		bindingDescription.stride = sizeof(Vertex);  /// Size of each vertex
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  /// Move to next vertex

		return bindingDescription;
	}

	/// Get attribute descriptions for this vertex format
	/// This describes how to extract vertex attributes from the vertex buffer
	/// @return Vector of attribute descriptions
	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

		/// Position attribute (location 0)
		attributeDescriptions[0].binding = 0;  /// Comes from the same buffer as binding 0
		attributeDescriptions[0].location = 0;  /// Location in the vertex shader
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  /// vec3
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		/// Normal attribute (location 1)
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;  /// vec3
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		/// Tangent attribute (location 2)
		/// This is used for normal mapping to construct the TBN matrix
		/// The tangent vector defines the positive X axis in tangent space
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;  /// vec3
		attributeDescriptions[2].offset = offsetof(Vertex, tangent);

		/// Color attribute (location 3)
		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;  /// vec3
		attributeDescriptions[3].offset = offsetof(Vertex, color);

		/// Texture coordinate attribute (location 4)
		/// This is a new attribute for texture mapping support
		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 4;  /// Location 4 in the vertex shader
		attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;  /// vec2 (U,V coordinates)
		attributeDescriptions[4].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	/// Equality operator for vertex comparison
	/// This is useful for mesh optimization to detect duplicate vertices
	/// @param other The vertex to compare against
	/// @return True if the vertices have identical attributes
	bool operator==(const Vertex& other) const {
		return position == other.position &&
			   normal == other.normal &&
			   tangent == other.tangent &&
			   color == other.color &&
			   texCoord == other.texCoord;
	}
};

} /// namespace lillugsi::rendering