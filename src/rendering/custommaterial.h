#pragma once

#include "material.h"
#include "vulkan/vulkanwrappers.h"

#include <string>
#include <unordered_map>

namespace lillugsi::rendering {

/// CustomMaterial allows for complete shader and uniform customization
/// While PBRMaterial provides a standard material workflow, CustomMaterial
/// enables users to define their own shading models and material properties
/// There is no Pipeline yet to use this material
class CustomMaterial : public Material {
public:
	/// Create a custom material with specified shaders
	/// @param device Logical device for resource creation
	/// @param name Unique identifier for this material
	/// @param physicalDevice Physical device for memory allocation
	/// @param vertexShaderPath Path to vertex shader SPIR-V file
	/// @param fragmentShaderPath Path to fragment shader SPIR-V file
	CustomMaterial(
		VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice,
		const std::string& vertexShaderPath,
		const std::string& fragmentShaderPath
	);

	~CustomMaterial() override;

	/// Get the shader paths for this custom material
	/// @return Shader paths configuration for custom pipeline creation
	[[nodiscard]] ShaderPaths getShaderPaths() const override;

	/// Define a new uniform buffer in the material
	/// @param name Identifier for the uniform buffer
	/// @param size Size of the uniform data in bytes
	/// @param stages Shader stages that will access this uniform
	void defineUniformBuffer(
		const std::string& name,
		VkDeviceSize size,
		VkShaderStageFlags stages
	);

	/// Update data in a uniform buffer
	/// @param name Name of the uniform buffer to update
	/// @param data Pointer to the new data
	/// @param size Size of the data in bytes
	/// @param offset Offset into the buffer (default 0)
	void updateUniformBuffer(
		const std::string& name,
		const void* data,
		VkDeviceSize size,
		VkDeviceSize offset = 0
	);

private:
	/// Structure to track uniform buffer information
	struct UniformBufferInfo {
		vulkan::VulkanBufferHandle buffer;
		VkDeviceMemory memory;
		VkDeviceSize size;
		VkShaderStageFlags stages;
		uint32_t binding;
	};

	/// Create the descriptor set layout based on uniform definitions
	void createDescriptorSetLayout();

	/// Create descriptor pool and sets
	void createDescriptorSets();

	/// Validate uniform buffer updates
	/// @throws VulkanException if validation fails
	void validateUniformUpdate( const std::string& name, VkDeviceSize size, VkDeviceSize offset ) const;

	/// Track uniform buffers and their metadata
	std::unordered_map<std::string, UniformBufferInfo> uniformBuffers;
	uint32_t nextBinding{0};  /// Track next available binding point

	/// Shader paths stored for pipeline creation
	std::string vertexShaderPath;
	std::string fragmentShaderPath;
};

} /// namespace lillugsi::rendering