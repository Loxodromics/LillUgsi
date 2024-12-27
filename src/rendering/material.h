#pragma once

#include "vulkan/vulkanwrappers.h"
#include <string>
#include <memory>

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
	virtual void bind(VkCommandBuffer cmdBuffer) const = 0;

	/// Get the descriptor set layout for this material type
	/// Each material type can have its own unique set of descriptors
	/// @return The descriptor set layout for this material
	[[nodiscard]] virtual VkDescriptorSetLayout getDescriptorSetLayout() const = 0;

	/// Get the name of this material
	/// This allows for material lookup and management
	/// @return The material's unique name
	[[nodiscard]] virtual std::string getName() const = 0;

	/// Disable copying to prevent multiple materials sharing the same GPU resources
	Material(const Material&) = delete;
	Material& operator=(const Material&) = delete;

protected:
	/// Protected constructor ensures only derived classes can be instantiated
	Material() = default;
};

} /// namespace lillugsi::rendering