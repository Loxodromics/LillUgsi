#pragma once

#include "vulkan/buffer.h"
#include "vulkan/commandbuffermanager.h"
#include "vulkan/pipelinemanager.h"
#include "vulkan/vulkanwrappers.h"

#include <memory>
#include <string>

namespace lillugsi::rendering {

class BufferManager;

/// ComputeTest verifies the compute pipeline infrastructure works correctly
///
/// This class provides a self-contained test that:
/// 1. Creates a storage buffer
/// 2. Dispatches a compute shader to write a predictable pattern
/// 3. Verifies the results on CPU
///
/// Purpose:
/// - Validates compute pipeline creation from Task 1
/// - Demonstrates storage buffer usage with compute shaders
/// - Teaches memory barriers between compute and host access
///
/// The test writes values[i] = i * 2.0 for 256 elements, then verifies
/// each element on CPU after dispatch completes.
///
/// Usage:
/// @code
/// ComputeTest test(device, physicalDevice, bufferManager, pipelineManager,
///                  commandBufferManager, graphicsQueue, queueFamilyIndex);
/// test.initialize();
/// bool success = test.runTest();
/// test.cleanup();
/// @endcode

class ComputeTest {
public:
	/// Constructor
	/// @param device The logical Vulkan device
	/// @param physicalDevice The physical device for memory queries
	/// @param bufferManager Buffer manager for storage buffer creation
	/// @param pipelineManager Pipeline manager for compute pipeline
	/// @param commandBufferManager Command buffer manager for dispatch
	/// @param graphicsQueue Queue for compute submission (graphics queues support compute)
	/// @param queueFamilyIndex Queue family index for command pool creation
	ComputeTest(
		VkDevice device,
		VkPhysicalDevice physicalDevice,
		BufferManager* bufferManager,
		vulkan::PipelineManager* pipelineManager,
		std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager,
		VkQueue graphicsQueue,
		uint32_t queueFamilyIndex);

	/// Destructor
	~ComputeTest();

	/// Initialize test resources
	/// Creates descriptor layout, pool, set, storage buffer, and compute pipeline
	/// @return True if initialization succeeded
	bool initialize();

	/// Run the compute test
	/// Dispatches compute shader and verifies results
	/// @return True if all values match expected pattern
	[[nodiscard]] bool runTest();

	/// Clean up all test resources
	void cleanup();

private:
	/// Create the descriptor set layout for storage buffer binding
	/// @return True if creation succeeded
	bool createDescriptorSetLayout();

	/// Create the descriptor pool
	/// @return True if creation succeeded
	bool createDescriptorPool();

	/// Allocate and configure the descriptor set
	/// @return True if allocation succeeded
	bool createDescriptorSet();

	/// Create the storage buffer for compute output
	/// @return True if creation succeeded
	bool createStorageBuffer();

	/// Create the compute pipeline using PipelineManager
	/// @return True if creation succeeded
	bool createComputePipeline();

	/// Dispatch compute shader and wait for completion
	void dispatchCompute();

	/// Verify buffer contents match expected pattern
	/// @return True if all values are correct
	[[nodiscard]] bool verifyResults();

	/// Vulkan handles
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue graphicsQueue;
	uint32_t queueFamilyIndex;

	/// External dependencies (not owned)
	BufferManager* bufferManager;
	vulkan::PipelineManager* pipelineManager;
	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager;

	/// Owned resources
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;

	/// Storage buffer for compute output
	std::shared_ptr<vulkan::Buffer> storageBuffer;

	/// Compute pipeline (managed by PipelineManager, we just hold a reference)
	std::shared_ptr<vulkan::VulkanPipelineHandle> computePipeline;

	/// Test configuration
	static constexpr uint32_t kElementCount = 256;
	static constexpr uint32_t kWorkGroupSize = 64;
	static constexpr const char* kPipelineName = "storage_buffer_test";
	static constexpr const char* kShaderPath = "shaders/storage_test.comp.spv";
};

} /// namespace lillugsi::rendering
