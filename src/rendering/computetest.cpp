#include "computetest.h"
#include "buffermanager.h"
#include "vulkan/vulkanexception.h"

#include <spdlog/spdlog.h>
#include <cmath>

namespace lillugsi::rendering {

ComputeTest::ComputeTest(
	VkDevice device,
	VkPhysicalDevice physicalDevice,
	BufferManager* bufferManager,
	vulkan::PipelineManager* pipelineManager,
	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager,
	VkQueue graphicsQueue,
	uint32_t queueFamilyIndex)
	: device(device)
	, physicalDevice(physicalDevice)
	, bufferManager(bufferManager)
	, pipelineManager(pipelineManager)
	, commandBufferManager(commandBufferManager)
	, graphicsQueue(graphicsQueue)
	, queueFamilyIndex(queueFamilyIndex) {
}

ComputeTest::~ComputeTest() {
	this->cleanup();
}

bool ComputeTest::initialize() {
	spdlog::info("ComputeTest: Initializing storage buffer test...");

	/// Create resources in dependency order
	if (!this->createDescriptorSetLayout()) {
		spdlog::error("ComputeTest: Failed to create descriptor set layout");
		return false;
	}

	if (!this->createStorageBuffer()) {
		spdlog::error("ComputeTest: Failed to create storage buffer");
		return false;
	}

	if (!this->createDescriptorPool()) {
		spdlog::error("ComputeTest: Failed to create descriptor pool");
		return false;
	}

	if (!this->createDescriptorSet()) {
		spdlog::error("ComputeTest: Failed to create descriptor set");
		return false;
	}

	if (!this->createComputePipeline()) {
		spdlog::error("ComputeTest: Failed to create compute pipeline");
		return false;
	}

	/// Create command pool for compute dispatch using CommandBufferManager
	/// This ensures the pool is tracked by the manager
	this->commandPool = this->commandBufferManager->createCommandPool(
		this->queueFamilyIndex,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	spdlog::info("ComputeTest: Initialization complete");
	return true;
}

bool ComputeTest::createDescriptorSetLayout() {
	/// Define the binding for our storage buffer
	/// This tells Vulkan: "At set 0, binding 0, there will be a storage buffer
	/// accessible from compute shaders"
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;  /// Matches layout(set = 0, binding = 0) in shader
	binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;  /// Not UNIFORM_BUFFER!
	binding.descriptorCount = 1;  /// One buffer
	binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;  /// Only compute shader uses it
	binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	VkResult result = vkCreateDescriptorSetLayout(
		this->device, &layoutInfo, nullptr, &this->descriptorSetLayout);

	if (result != VK_SUCCESS) {
		spdlog::error("ComputeTest: vkCreateDescriptorSetLayout failed: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("ComputeTest: Created descriptor set layout for storage buffer");
	return true;
}

bool ComputeTest::createStorageBuffer() {
	/// Calculate buffer size: 256 floats = 1024 bytes
	VkDeviceSize bufferSize = kElementCount * sizeof(float);

	/// Use existing BufferManager - it already supports storage buffers!
	/// This creates a buffer with:
	/// - VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
	/// - VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	this->storageBuffer = this->bufferManager->createStorageBuffer(bufferSize);

	if (!this->storageBuffer) {
		spdlog::error("ComputeTest: Failed to create storage buffer");
		return false;
	}

	spdlog::debug("ComputeTest: Created storage buffer ({} bytes for {} floats)",
		bufferSize, kElementCount);
	return true;
}

bool ComputeTest::createDescriptorPool() {
	/// Pool needs to support storage buffer descriptors
	/// We only need 1 descriptor of type STORAGE_BUFFER
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;  /// We only allocate 1 descriptor set

	VkResult result = vkCreateDescriptorPool(
		this->device, &poolInfo, nullptr, &this->descriptorPool);

	if (result != VK_SUCCESS) {
		spdlog::error("ComputeTest: vkCreateDescriptorPool failed: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("ComputeTest: Created descriptor pool");
	return true;
}

bool ComputeTest::createDescriptorSet() {
	/// Allocate descriptor set from the pool using our layout
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &this->descriptorSetLayout;

	VkResult result = vkAllocateDescriptorSets(
		this->device, &allocInfo, &this->descriptorSet);

	if (result != VK_SUCCESS) {
		spdlog::error("ComputeTest: vkAllocateDescriptorSets failed: {}", static_cast<int>(result));
		return false;
	}

	/// Now we need to tell the descriptor set which buffer to use
	/// This "binds" our storage buffer to the descriptor
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = this->storageBuffer->get();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;  /// Use entire buffer

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = this->descriptorSet;
	descriptorWrite.dstBinding = 0;  /// Binding 0
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo = &bufferInfo;

	/// Update the descriptor set with our buffer binding
	vkUpdateDescriptorSets(this->device, 1, &descriptorWrite, 0, nullptr);

	spdlog::debug("ComputeTest: Created and configured descriptor set");
	return true;
}

bool ComputeTest::createComputePipeline() {
	/// Use PipelineManager from Task 1 to create the compute pipeline
	/// This handles shader loading, pipeline layout creation, etc.
	this->computePipeline = this->pipelineManager->createComputePipeline(
		kPipelineName,
		kShaderPath,
		this->descriptorSetLayout);

	if (!this->computePipeline) {
		spdlog::error("ComputeTest: Failed to create compute pipeline");
		return false;
	}

	spdlog::debug("ComputeTest: Created compute pipeline '{}'", kPipelineName);
	return true;
}

void ComputeTest::dispatchCompute() {
	/// Begin recording a single-time command buffer
	VkCommandBuffer cmd = this->commandBufferManager->beginSingleTimeCommands(this->commandPool);

	/// Get pipeline layout from PipelineManager
	auto pipelineLayout = this->pipelineManager->getComputePipelineLayout(kPipelineName);

	/// Bind the compute pipeline
	/// Note: VK_PIPELINE_BIND_POINT_COMPUTE, not GRAPHICS!
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, this->computePipeline->get());

	/// Bind descriptor sets
	/// Again, VK_PIPELINE_BIND_POINT_COMPUTE
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		pipelineLayout->get(),
		0,              /// First set (set = 0)
		1,              /// One set
		&this->descriptorSet,
		0, nullptr);    /// No dynamic offsets

	/// Dispatch compute work
	/// 256 elements / 64 threads per work group = 4 work groups
	uint32_t groupCountX = (kElementCount + kWorkGroupSize - 1) / kWorkGroupSize;
	vkCmdDispatch(cmd, groupCountX, 1, 1);

	/// Memory barrier: ensure compute shader writes are visible to host (CPU)
	///
	/// Without this barrier, the CPU might read stale or incomplete data.
	/// This is the critical synchronisation step in compute shader usage.
	///
	/// srcAccessMask: What access we're waiting for (shader writes)
	/// dstAccessMask: What access needs to see those writes (host reads)
	/// srcStageMask: Which pipeline stage does the writing (compute)
	/// dstStageMask: Which stage needs the data (host - CPU)
	VkMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  /// Wait for compute to finish
		VK_PIPELINE_STAGE_HOST_BIT,             /// Before host reads
		0,              /// No dependency flags
		1, &memoryBarrier,  /// One memory barrier
		0, nullptr,     /// No buffer barriers
		0, nullptr);    /// No image barriers

	/// End and submit the command buffer
	/// This blocks until the GPU finishes (synchronous for simplicity)
	this->commandBufferManager->endSingleTimeCommands(
		cmd, this->commandPool, this->graphicsQueue);

	spdlog::debug("ComputeTest: Dispatched {} work groups ({} total threads)",
		groupCountX, groupCountX * kWorkGroupSize);
}

bool ComputeTest::verifyResults() {
	/// Map the buffer to read results on CPU
	void* mappedData = this->storageBuffer->map(0, VK_WHOLE_SIZE);
	if (!mappedData) {
		spdlog::error("ComputeTest: Failed to map storage buffer");
		return false;
	}

	float* values = static_cast<float*>(mappedData);
	bool success = true;
	uint32_t errorCount = 0;

	/// Verify each element matches expected pattern: value[i] = i * 2.0
	for (uint32_t i = 0; i < kElementCount; ++i) {
		float expected = static_cast<float>(i) * 2.0f;
		float actual = values[i];

		/// Use small epsilon for floating point comparison
		if (std::abs(actual - expected) > 0.001f) {
			if (errorCount < 5) {  /// Only log first 5 errors
				spdlog::error("ComputeTest: Mismatch at index {}: expected {}, got {}",
					i, expected, actual);
			}
			++errorCount;
			success = false;
		}
	}

	this->storageBuffer->unmap();

	if (success) {
		spdlog::info("ComputeTest: All {} values verified correctly!", kElementCount);
	} else {
		spdlog::error("ComputeTest: {} of {} values incorrect", errorCount, kElementCount);
	}

	return success;
}

bool ComputeTest::runTest() {
	spdlog::info("ComputeTest: Running storage buffer test...");

	/// Step 1: Dispatch compute shader
	this->dispatchCompute();

	/// Step 2: Verify results
	bool success = this->verifyResults();

	if (success) {
		spdlog::info("ComputeTest: Storage buffer test PASSED!");
	} else {
		spdlog::error("ComputeTest: Storage buffer test FAILED!");
	}

	return success;
}

void ComputeTest::cleanup() {
	if (this->device == VK_NULL_HANDLE) {
		return;
	}

	/// Wait for device to be idle before cleanup
	vkDeviceWaitIdle(this->device);

	/// Command pool is owned by CommandBufferManager, so we don't destroy it here
	/// Just clear our reference
	this->commandPool = VK_NULL_HANDLE;

	/// Destroy descriptor pool (this also frees descriptor sets)
	if (this->descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);
		this->descriptorPool = VK_NULL_HANDLE;
	}

	/// Destroy descriptor set layout
	if (this->descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(this->device, this->descriptorSetLayout, nullptr);
		this->descriptorSetLayout = VK_NULL_HANDLE;
	}

	/// Storage buffer and compute pipeline are managed by their respective managers
	/// so we just release our references
	this->storageBuffer.reset();
	this->computePipeline.reset();

	spdlog::debug("ComputeTest: Cleanup complete");
}

} /// namespace lillugsi::rendering
