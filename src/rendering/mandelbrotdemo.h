#pragma once

#include "vulkan/vulkanwrappers.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

/// Forward declarations
union SDL_Event;

namespace lillugsi::rendering {

/// Push constant structure matching the shader
/// Must match the layout in mandelbrot.comp.glsl
struct MandelbrotPushConstants {
	glm::vec2 offset;   /// Center point in complex plane
	float zoom;         /// Scale factor
	int maxIter;        /// Maximum iteration count
};

/// Minimal vertex format for fullscreen quad
/// Only position and texture coordinates needed
struct QuadVertex {
	glm::vec2 position;   /// NDC coordinates (-1 to 1)
	glm::vec2 texCoord;   /// UV coordinates (0 to 1)
};

/// MandelbrotDemo is a standalone compute shader learning project
///
/// This class demonstrates:
/// - Storage images for compute shader writes (VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
/// - imageStore() in GLSL compute shaders
/// - Image memory barriers between compute and fragment stages
/// - Push constants for interactive parameters
/// - Complete mini-pipeline: compute generates → graphics displays
///
/// Usage:
/// @code
/// MandelbrotDemo demo(device, physicalDevice);
/// demo.initialize();
/// // In command buffer recording:
/// demo.generate(cmd);  // Before render pass - dispatches compute
/// // Inside render pass:
/// demo.render(cmd);    // Draws fullscreen quad with fractal
/// @endcode
class MandelbrotDemo {
public:
	/// Constructor
	/// @param device The logical Vulkan device
	/// @param physicalDevice The physical device for memory queries
	MandelbrotDemo(VkDevice device, VkPhysicalDevice physicalDevice);

	/// Destructor
	~MandelbrotDemo();

	/// Initialize all resources
	/// @return True if initialization succeeded
	bool initialize();

	/// Clean up all resources
	void cleanup();

	/// Generate fractal (Step 7)
	/// Dispatches compute shader to generate Mandelbrot fractal
	/// Must be called BEFORE render pass begins
	/// @param cmd Command buffer to record into
	void generate(VkCommandBuffer cmd);

	/// Render fractal (Step 11)
	/// Draws fullscreen quad with fractal texture
	/// Must be called INSIDE render pass
	/// @param cmd Command buffer to record into
	void render(VkCommandBuffer cmd);

	/// Step 10: Create graphics pipeline and descriptors
	/// Creates pipeline for rendering the fullscreen quad with fractal texture
	/// Must be called after initialize() with the renderer's render pass
	/// @param renderPass The render pass to use for pipeline creation
	/// @return True if creation succeeded
	bool createGraphicsPipeline(VkRenderPass renderPass);

	/// Step 13: Handle input for interactive controls
	/// Processes SDL events to adjust fractal parameters
	/// Arrow keys: Pan (adjust offset)
	/// +/- keys: Zoom in/out
	/// R key: Reset to default view
	/// @param event The SDL event to process
	/// @return True if the event was handled (requires command buffer re-recording)
	bool handleInput(const SDL_Event& event);

	/// Get current fractal parameters
	/// Used for debugging and verification
	MandelbrotPushConstants getParameters() const { return this->params; }

private:
	/// Step 1: Create the storage image
	/// Creates a 512x512 RGBA8 image with STORAGE + SAMPLED usage
	/// @return True if creation succeeded
	bool createStorageImage();

	/// Step 2: Create the image view
	/// Creates a view for both compute storage access and fragment sampling
	/// @return True if creation succeeded
	bool createImageView();

	/// Step 3: Create compute descriptor layout
	/// Defines the layout for storage image binding at set=0, binding=0
	/// @return True if creation succeeded
	bool createComputeDescriptorLayout();

	/// Step 4: Create compute descriptor pool and set
	/// Allocates descriptor set and updates it with storage image
	/// @return True if creation succeeded
	bool createComputeDescriptorSet();

	/// Step 6: Create compute pipeline with push constants
	/// Creates pipeline layout with push constant range and compute pipeline
	/// @return True if creation succeeded
	bool createComputePipeline();

	/// Step 8: Create fullscreen quad geometry
	/// Creates vertex and index buffers for rendering a textured quad
	/// @return True if creation succeeded
	bool createQuadGeometry();

	/// Find suitable memory type for allocation
	/// @param typeFilter Bitmask of suitable memory types
	/// @param properties Required memory properties
	/// @return Memory type index
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	/// Vulkan handles
	VkDevice device;
	VkPhysicalDevice physicalDevice;

	/// Storage image resources (Step 1)
	VkImage storageImage = VK_NULL_HANDLE;
	VkDeviceMemory storageImageMemory = VK_NULL_HANDLE;

	/// Image view for both compute and fragment access (Step 2)
	VkImageView storageImageView = VK_NULL_HANDLE;

	/// Compute descriptor layout (Step 3)
	VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;

	/// Compute descriptor pool and set (Step 4)
	VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;

	/// Compute pipeline with push constants (Step 6)
	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	VkPipeline computePipeline = VK_NULL_HANDLE;

	/// Fullscreen quad geometry (Step 8)
	VkBuffer quadVertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory quadVertexMemory = VK_NULL_HANDLE;
	VkBuffer quadIndexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory quadIndexMemory = VK_NULL_HANDLE;

	/// Graphics pipeline and descriptors (Step 10)
	VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;

	/// Configuration
	static constexpr uint32_t kImageSize = 512;
	static constexpr VkFormat kImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	/// Interactive parameter tracking (Step 13)
	MandelbrotPushConstants params{
		.offset = glm::vec2(0.0f, 0.0f),   /// Center at origin
		.zoom = 3.0f,                       /// Initial zoom to see full set
		.maxIter = 256                      /// Good balance of detail and performance
	};

	/// Parameter adjustment speeds
	static constexpr float kPanSpeed = 0.1f;      /// Pan distance per key press (relative to zoom)
	static constexpr float kZoomSpeed = 1.2f;     /// Zoom multiplier (20% per key press)
	static constexpr int kIterStep = 32;          /// Iteration increment
};

} /// namespace lillugsi::rendering
