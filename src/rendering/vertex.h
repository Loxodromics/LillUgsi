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
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

		/// Position attribute
		attributeDescriptions[0].binding = 0;  /// Comes from the same buffer as binding 0
		attributeDescriptions[0].location = 0;  /// Location in the vertex shader
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  /// vec3
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		/// Normal attribute
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;  /// vec3
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		/// Color attribute
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;  /// vec3
		attributeDescriptions[2].offset = offsetof(Vertex, color);

		/// Texture coordinate attribute
		/// This is a new attribute for texture mapping support
		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;  /// Location 3 in the vertex shader
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;  /// vec2 (U,V coordinates)
		attributeDescriptions[3].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

} /// namespace lillugsi::rendering