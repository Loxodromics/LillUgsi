#pragma once

#include "vulkan/vulkanwrappers.h"
#include <string>

namespace lillugsi::rendering {

/// Base class for all materials in the rendering system
/// We use a pure virtual interface to allow for different material types
/// while maintaining a consistent interface for the renderer
class Material {
public:
	/// Virtual destructor ensures proper cleanup of derived classes
	virtual ~Material() = default;

	/// Bind this material's resources for rendering
	/// This includes setting up descriptor sets, push constants,
	/// and any other material-specific state
	/// @param cmdBuffer The command buffer to record binding commands to
	virtual void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const;

	/// Get the descriptor set layout for this material type
	/// Each material type can have its own unique set of descriptors
	/// @return The descriptor set layout for this material
	[[nodiscard]] virtual VkDescriptorSetLayout getDescriptorSetLayout() const;

	/// Get the name of this material
	/// This allows for material lookup and management
	/// @return The material's unique name
	[[nodiscard]] virtual std::string getName() const { return this->name; };

	/// Disable copying to prevent multiple materials sharing the same GPU resources
	Material(const Material&) = delete;
	Material& operator=(const Material&) = delete;

protected:
	/// Create a new Material, but only for subclasses
	/// @param device The logical device for creating GPU resources
	/// @param name Unique name for this material instance
	/// @param physicalDevice The logical device for findMemoryType
	Material(VkDevice device, const std::string& name, VkPhysicalDevice physicalDevice);

	/// Protected constructor ensures only derived classes can be instantiated
	Material() = default;

	/// Create a descriptor pool for material descriptors
	/// We create a dedicated pool for this material's descriptors
	/// to simplify resource management and lifetime
	void createDescriptorPool();

	VkDevice device;                 /// Logical device reference
	VkPhysicalDevice physicalDevice; /// Physical device reference
	std::string name;                /// Unique material name

	/// GPU resources
	vulkan::VulkanDescriptorSetLayoutHandle descriptorSetLayout;
	vulkan::VulkanBufferHandle uniformBuffer;
	VkDeviceMemory uniformBufferMemory;  /// Memory for uniform buffer
	VkDescriptorSet descriptorSet;       /// Descriptor set for binding
	vulkan::VulkanDescriptorPoolHandle descriptorPool;
};

} /// namespace lillugsi::rendering