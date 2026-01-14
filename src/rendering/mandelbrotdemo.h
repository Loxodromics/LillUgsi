#pragma once

#include "vulkan/vulkanwrappers.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>

/// Forward declarations
union SDL_Event;

namespace lillugsi::vulkan {
class IndexBuffer;
}

namespace lillugsi::rendering {

class BufferManager;

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

/// MandelbrotDemo demonstrates modern Vulkan resource management
///
/// This class demonstrates:
/// - RAII wrappers (VulkanHandle) for automatic cleanup
/// - BufferManager integration for efficient buffer creation
/// - Storage images for compute shader writes (VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
/// - Image memory barriers between compute and fragment stages
/// - Push constants for interactive parameters
/// - ShaderModule::fromSpirV() for simplified shader loading
/// - Complete mini-pipeline: compute generates → graphics displays
///
/// Resource Management:
/// - Most handles use RAII wrappers for automatic cleanup
/// - VkDescriptorSet remains raw (freed by pool destruction)
/// - VkDeviceMemory remains raw (consistent with Texture pattern)
/// - Buffers use shared_ptr from BufferManager
///
/// Usage:
/// @code
/// MandelbrotDemo demo(device, physicalDevice, bufferManager);
/// demo.initialize(width, height);
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
	/// @param bufferManager Buffer manager for creating quad geometry
	MandelbrotDemo(VkDevice device, VkPhysicalDevice physicalDevice, BufferManager* bufferManager);

	/// Destructor
	~MandelbrotDemo();

	/// Initialize all resources
	/// @param width Width of the storage image and viewport
	/// @param height Height of the storage image and viewport
	/// @return True if initialization succeeded
	bool initialize(uint32_t width, uint32_t height);

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
	BufferManager* bufferManager;  /// Buffer manager for creating quad geometry (not owned)

	/// Storage image resources (Step 1)
	vulkan::VulkanImageHandle storageImage;
	VkDeviceMemory storageImageMemory{VK_NULL_HANDLE};  /// Keep raw for consistency with Texture pattern

	/// Image view for both compute and fragment access (Step 2)
	vulkan::VulkanImageViewHandle storageImageView;

	/// Compute descriptor layout (Step 3)
	vulkan::VulkanDescriptorSetLayoutHandle computeDescriptorSetLayout;

	/// Compute descriptor pool and set (Step 4)
	vulkan::VulkanDescriptorPoolHandle computeDescriptorPool;
	VkDescriptorSet computeDescriptorSet{VK_NULL_HANDLE};  /// Keep raw (freed when pool destroyed)

	/// Compute pipeline with push constants (Step 6)
	vulkan::VulkanPipelineLayoutHandle computePipelineLayout;
	vulkan::VulkanPipelineHandle computePipeline;

	/// Fullscreen quad geometry (Step 8)
	/// Note: QuadVertex format differs from standard Vertex, so vertex buffer created manually
	/// Index buffer uses BufferManager since it's just uint32_t indices
	vulkan::VulkanBufferHandle quadVertexBuffer;
	VkDeviceMemory quadVertexMemory{VK_NULL_HANDLE};  /// Keep raw for consistency
	std::shared_ptr<vulkan::IndexBuffer> quadIndexBuffer;

	/// Graphics pipeline and descriptors (Step 10)
	vulkan::VulkanDescriptorSetLayoutHandle graphicsDescriptorSetLayout;
	vulkan::VulkanDescriptorPoolHandle graphicsDescriptorPool;
	VkDescriptorSet graphicsDescriptorSet{VK_NULL_HANDLE};  /// Keep raw (freed when pool destroyed)
	vulkan::VulkanSamplerHandle sampler;
	vulkan::VulkanPipelineLayoutHandle graphicsPipelineLayout;
	vulkan::VulkanPipelineHandle graphicsPipeline;

	/// Configuration
	uint32_t imageWidth{0};    /// Width of storage image (dynamic, set from swap chain)
	uint32_t imageHeight{0};   /// Height of storage image (dynamic, set from swap chain)
	static constexpr VkFormat kImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	/// Shader paths for automated loading
	static constexpr const char* kComputeShaderPath = "shaders/mandelbrot.comp.spv";
	static constexpr const char* kVertexShaderPath = "shaders/fullscreenquad.vert.spv";
	static constexpr const char* kFragmentShaderPath = "shaders/fullscreenquad.frag.spv";

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
