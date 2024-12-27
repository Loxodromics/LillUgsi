#pragma once

#include "material.h"
#include "vulkan/vulkanwrappers.h"
#include "vulkan/shaderprogram.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace lillugsi::rendering {

/// CustomMaterial allows for complete shader and uniform customization
/// While PBRMaterial provides a standard material workflow, CustomMaterial
/// enables users to define their own shading models and material properties
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

	void bind(VkCommandBuffer cmdBuffer) const override;
	[[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const override;
	[[nodiscard]] std::string getName() const override { return this->name; }

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

	/// Get the shader program used by this material
	/// @return The material's shader program
	[[nodiscard]] const vulkan::ShaderProgram& getShaderProgram() const {
		return this->shaderProgram;
	}

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
	void validateUniformUpdate(
		const std::string& name,
		VkDeviceSize size,
		VkDeviceSize offset
	) const;

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	std::string name;
	vulkan::ShaderProgram shaderProgram;

	/// GPU resource management
	vulkan::VulkanDescriptorSetLayoutHandle descriptorSetLayout;
	vulkan::VulkanDescriptorPoolHandle descriptorPool;
	VkDescriptorSet descriptorSet;

	/// Track uniform buffers and their metadata
	std::unordered_map<std::string, UniformBufferInfo> uniformBuffers;
	uint32_t nextBinding{0};  /// Track next available binding point
};

} /// namespace lillugsi::rendering