#include "mandelbrotdemo.h"

#include "vulkan/vulkanexception.h"

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace lillugsi::rendering {

MandelbrotDemo::MandelbrotDemo(VkDevice device, VkPhysicalDevice physicalDevice)
	: device(device), physicalDevice(physicalDevice) {
}

MandelbrotDemo::~MandelbrotDemo() {
	this->cleanup();
}

bool MandelbrotDemo::initialize() {
	spdlog::info("Initializing MandelbrotDemo");

	if (!this->createStorageImage()) {
		spdlog::error("Failed to create storage image");
		return false;
	}

	spdlog::info("Created storage image ({}x{} RGBA8)", kImageSize, kImageSize);

	if (!this->createImageView()) {
		spdlog::error("Failed to create image view");
		return false;
	}

	spdlog::info("Created image view");

	if (!this->createComputeDescriptorLayout()) {
		spdlog::error("Failed to create compute descriptor layout");
		return false;
	}

	spdlog::info("Created compute descriptor layout (STORAGE_IMAGE)");

	if (!this->createComputeDescriptorSet()) {
		spdlog::error("Failed to create compute descriptor set");
		return false;
	}

	spdlog::info("Created compute descriptor set (GENERAL layout)");

	if (!this->createComputePipeline()) {
		spdlog::error("Failed to create compute pipeline");
		return false;
	}

	spdlog::info("Created compute pipeline with push constants");

	if (!this->createQuadGeometry()) {
		spdlog::error("Failed to create quad geometry");
		return false;
	}

	spdlog::info("Created quad geometry (4 vertices, 6 indices)");

	/// Note: Graphics pipeline is NOT created here
	/// It will be created separately via createGraphicsPipeline(renderPass)
	/// after the renderer has created its render pass

	return true;
}

bool MandelbrotDemo::createStorageImage() {
	/// Create image info
	/// Key usage flags:
	/// - STORAGE_BIT: Allows compute shader to write via imageStore()
	/// - SAMPLED_BIT: Allows fragment shader to read via texture()
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = kImageFormat;
	imageInfo.extent.width = kImageSize;
	imageInfo.extent.height = kImageSize;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT |   /// For compute write
					  VK_IMAGE_USAGE_SAMPLED_BIT;    /// For fragment read
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	/// Create the image
	VkResult result = vkCreateImage(this->device, &imageInfo, nullptr, &this->storageImage);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create storage image: {}", static_cast<int>(result));
		return false;
	}

	/// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(this->device, this->storageImage, &memRequirements);

	/// Allocate memory
	/// DEVICE_LOCAL for best GPU performance
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	result = vkAllocateMemory(this->device, &allocInfo, nullptr, &this->storageImageMemory);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to allocate storage image memory: {}", static_cast<int>(result));
		return false;
	}

	/// Bind memory to image
	vkBindImageMemory(this->device, this->storageImage, this->storageImageMemory, 0);

	spdlog::debug("Storage image created with STORAGE_BIT + SAMPLED_BIT usage");

	return true;
}

bool MandelbrotDemo::createImageView() {
	/// Create image view for both compute and fragment access
	/// Single view can be used for:
	/// - Compute shader writes (storage image in descriptor)
	/// - Fragment shader reads (combined image sampler in descriptor)
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = this->storageImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = kImageFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkResult result = vkCreateImageView(this->device, &viewInfo, nullptr, &this->storageImageView);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create image view: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("Image view created for both compute and fragment access");

	return true;
}

bool MandelbrotDemo::createComputeDescriptorLayout() {
	/// Create descriptor set layout for compute shader
	/// Key difference from sampled images:
	/// - Use VK_DESCRIPTOR_TYPE_STORAGE_IMAGE (not COMBINED_IMAGE_SAMPLER)
	/// - Allows imageStore() writes in compute shader
	/// - Matches shader: layout(rgba8, set=0, binding=0) writeonly image2D
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = 0;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &layoutBinding;

	VkResult result = vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr,
												   &this->computeDescriptorSetLayout);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create compute descriptor set layout: {}",
					  static_cast<int>(result));
		return false;
	}

	spdlog::debug("Compute descriptor layout created (STORAGE_IMAGE at binding 0)");

	return true;
}

bool MandelbrotDemo::createComputeDescriptorSet() {
	/// Step 4a: Create descriptor pool
	/// Pool must support VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	VkResult result = vkCreateDescriptorPool(this->device, &poolInfo, nullptr,
											 &this->computeDescriptorPool);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create compute descriptor pool: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("Compute descriptor pool created");

	/// Step 4b: Allocate descriptor set from pool
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->computeDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &this->computeDescriptorSetLayout;

	result = vkAllocateDescriptorSets(this->device, &allocInfo, &this->computeDescriptorSet);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to allocate compute descriptor set: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("Compute descriptor set allocated");

	/// Step 4c: Update descriptor set with storage image
	/// Key difference: VK_IMAGE_LAYOUT_GENERAL (not SHADER_READ_ONLY_OPTIMAL)
	/// GENERAL layout is required for storage images in compute shaders
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = this->storageImageView;
	imageInfo.sampler = VK_NULL_HANDLE;  /// No sampler needed for storage images

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = this->computeDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(this->device, 1, &descriptorWrite, 0, nullptr);

	spdlog::debug("Descriptor set updated with storage image (GENERAL layout)");

	return true;
}

bool MandelbrotDemo::createComputePipeline() {
	/// Step 6a: Define push constant range
	/// Push constants allow updating shader parameters without recreating descriptors
	/// Size must match MandelbrotPushConstants struct
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(MandelbrotPushConstants);

	/// Step 6b: Create pipeline layout
	/// Combines descriptor set layouts with push constant ranges
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &this->computeDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	VkResult result = vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr,
											 &this->computePipelineLayout);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create compute pipeline layout: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("Compute pipeline layout created with push constants (size={})",
				  sizeof(MandelbrotPushConstants));

	/// Step 6c: Load compute shader module
	/// Read SPIR-V bytecode from compiled shader
	std::ifstream file("shaders/mandelbrot.comp.spv", std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		spdlog::error("Failed to open shader file: shaders/mandelbrot.comp.spv");
		return false;
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> code(fileSize);
	file.seekg(0);
	file.read(code.data(), fileSize);
	file.close();

	VkShaderModuleCreateInfo shaderModuleInfo{};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.codeSize = code.size();
	shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	result = vkCreateShaderModule(this->device, &shaderModuleInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create shader module: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("Loaded compute shader module ({} bytes)", fileSize);

	/// Step 6d: Create compute pipeline
	/// Simpler than graphics pipeline - just one shader stage
	VkPipelineShaderStageCreateInfo shaderStageInfo{};
	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStageInfo.module = shaderModule;
	shaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = shaderStageInfo;
	pipelineInfo.layout = this->computePipelineLayout;

	result = vkCreateComputePipelines(this->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
									  &this->computePipeline);

	/// Shader module can be destroyed after pipeline creation
	vkDestroyShaderModule(this->device, shaderModule, nullptr);

	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create compute pipeline: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("Compute pipeline created");

	return true;
}

void MandelbrotDemo::generate(VkCommandBuffer cmd) {
	/// Step 7: Dispatch compute shader with image barriers
	/// This demonstrates the complete compute workflow:
	/// 1. Transition image to GENERAL layout for compute writes
	/// 2. Bind pipeline and descriptors
	/// 3. Update push constants
	/// 4. Dispatch compute work
	/// 5. Transition image to SHADER_READ_ONLY for fragment reads

	/// Step 7a: Barrier 1 - UNDEFINED → GENERAL
	/// Prepare image for compute shader writes via imageStore()
	VkImageMemoryBarrier barrier1{};
	barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier1.image = this->storageImage;
	barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier1.subresourceRange.baseMipLevel = 0;
	barrier1.subresourceRange.levelCount = 1;
	barrier1.subresourceRange.baseArrayLayer = 0;
	barrier1.subresourceRange.layerCount = 1;
	barrier1.srcAccessMask = 0;  /// No previous access
	barrier1.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;  /// Compute will write

	/// Pipeline barrier: TOP_OF_PIPE → COMPUTE_SHADER
	/// This ensures the image is ready before compute shader runs
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,      /// Wait for nothing (image is new)
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   /// Block compute shader stage
		0,                                       /// No dependency flags
		0, nullptr,                              /// No memory barriers
		0, nullptr,                              /// No buffer barriers
		1, &barrier1                             /// One image barrier
	);

	/// Step 7b: Bind compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, this->computePipeline);

	/// Step 7c: Bind descriptor set (contains storage image)
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		this->computePipelineLayout,
		0,                              /// First set
		1,                              /// One set
		&this->computeDescriptorSet,
		0, nullptr                      /// No dynamic offsets
	);

	/// Step 7d: Update push constants
	/// These parameters control the fractal generation
	/// Now using member variable that can be modified via handleInput()
	vkCmdPushConstants(
		cmd,
		this->computePipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,                              /// Offset
		sizeof(MandelbrotPushConstants),
		&this->params                   /// Use member variable (Step 13)
	);

	/// Step 7e: Dispatch compute work
	/// Work groups: 512 / 16 = 32 groups in each dimension
	/// Total: 32x32 = 1024 work groups
	/// Each work group has 16x16 = 256 threads
	/// Total threads: 1024 * 256 = 262,144 threads (one per pixel)
	constexpr uint32_t workGroupSize = 16;  /// Matches shader local_size_x/y
	const uint32_t groupCountX = (kImageSize + workGroupSize - 1) / workGroupSize;
	const uint32_t groupCountY = (kImageSize + workGroupSize - 1) / workGroupSize;

	vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

	/// Step 7f: Barrier 2 - GENERAL → SHADER_READ_ONLY_OPTIMAL
	/// Prepare image for fragment shader sampling via texture()
	VkImageMemoryBarrier barrier2{};
	barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier2.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier2.image = this->storageImage;
	barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier2.subresourceRange.baseMipLevel = 0;
	barrier2.subresourceRange.levelCount = 1;
	barrier2.subresourceRange.baseArrayLayer = 0;
	barrier2.subresourceRange.layerCount = 1;
	barrier2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;  /// Compute wrote
	barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;   /// Fragment will read

	/// Pipeline barrier: COMPUTE_SHADER → FRAGMENT_SHADER
	/// This ensures compute writes are finished before fragment reads
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,    /// Wait for compute to finish
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,   /// Block fragment shader stage
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier2
	);

	spdlog::debug("Dispatched Mandelbrot compute ({}x{} work groups)", groupCountX, groupCountY);
}

void MandelbrotDemo::render(VkCommandBuffer cmd) {
	/// Step 11: Render Fullscreen Quad
	///
	/// This method demonstrates:
	/// - Binding graphics pipeline
	/// - Setting dynamic viewport and scissor state
	/// - Binding descriptor sets with sampled texture
	/// - Vertex and index buffer binding
	/// - Indexed drawing
	///
	/// Must be called INSIDE a render pass after generate() has been called

	/// Part 1: Bind graphics pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphicsPipeline);

	/// Part 2: Set dynamic viewport and scissor
	/// These are dynamic states, so we set them at draw time rather than pipeline creation
	/// We'll use the fractal image size (512x512) as our viewport size
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(kImageSize);
	viewport.height = static_cast<float>(kImageSize);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = {kImageSize, kImageSize};
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	/// Part 3: Bind descriptor set with fractal texture
	/// set=0 contains the COMBINED_IMAGE_SAMPLER pointing to our fractal image
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		this->graphicsPipelineLayout,
		0,                                /// First set (set=0)
		1,                                /// Bind 1 descriptor set
		&this->graphicsDescriptorSet,
		0, nullptr                        /// No dynamic offsets
	);

	/// Part 4: Bind vertex buffer
	/// Our quad has 4 vertices (QuadVertex = position + texCoord)
	VkBuffer vertexBuffers[] = {this->quadVertexBuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

	/// Part 5: Bind index buffer
	/// Our quad uses 6 indices (2 triangles) with UINT32 indices
	vkCmdBindIndexBuffer(cmd, this->quadIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	/// Part 6: Draw indexed
	/// Draw 6 indices (2 triangles), 1 instance, starting at index 0, vertex offset 0
	vkCmdDrawIndexed(
		cmd,
		6,                /// Index count (2 triangles × 3 vertices)
		1,                /// Instance count
		0,                /// First index
		0,                /// Vertex offset
		0                 /// First instance
	);

	spdlog::debug("Rendered fullscreen quad with Mandelbrot fractal");
}

bool MandelbrotDemo::createQuadGeometry() {
	/// Step 8: Create fullscreen quad for displaying the fractal
	/// Quad covers entire screen in normalized device coordinates (NDC)
	/// NDC range: (-1,-1) bottom-left to (1,1) top-right
	/// UV range: (0,0) bottom-left to (1,1) top-right

	/// Define 4 vertices for fullscreen quad
	/// Position in NDC, texCoords in UV space
	std::vector<QuadVertex> vertices = {
		{{-1.0f, -1.0f}, {0.0f, 0.0f}},  /// Bottom-left
		{{ 1.0f, -1.0f}, {1.0f, 0.0f}},  /// Bottom-right
		{{ 1.0f,  1.0f}, {1.0f, 1.0f}},  /// Top-right
		{{-1.0f,  1.0f}, {0.0f, 1.0f}}   /// Top-left
	};

	/// Define 6 indices for 2 triangles (quad)
	/// Triangle 1: 0-1-2 (bottom-left, bottom-right, top-right)
	/// Triangle 2: 2-3-0 (top-right, top-left, bottom-left)
	std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

	/// Calculate buffer sizes
	VkDeviceSize vertexBufferSize = sizeof(QuadVertex) * vertices.size();
	VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

	/// Create vertex buffer
	VkBufferCreateInfo vertexBufferInfo{};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.size = vertexBufferSize;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(this->device, &vertexBufferInfo, nullptr,
									 &this->quadVertexBuffer);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create quad vertex buffer: {}", static_cast<int>(result));
		return false;
	}

	/// Allocate vertex buffer memory
	VkMemoryRequirements vertexMemReq;
	vkGetBufferMemoryRequirements(this->device, this->quadVertexBuffer, &vertexMemReq);

	VkMemoryAllocateInfo vertexAllocInfo{};
	vertexAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vertexAllocInfo.allocationSize = vertexMemReq.size;
	vertexAllocInfo.memoryTypeIndex = this->findMemoryType(
		vertexMemReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	result = vkAllocateMemory(this->device, &vertexAllocInfo, nullptr, &this->quadVertexMemory);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to allocate quad vertex memory: {}", static_cast<int>(result));
		return false;
	}

	/// Copy vertex data to buffer
	void* vertexData;
	vkMapMemory(this->device, this->quadVertexMemory, 0, vertexBufferSize, 0, &vertexData);
	memcpy(vertexData, vertices.data(), static_cast<size_t>(vertexBufferSize));
	vkUnmapMemory(this->device, this->quadVertexMemory);

	vkBindBufferMemory(this->device, this->quadVertexBuffer, this->quadVertexMemory, 0);

	/// Create index buffer
	VkBufferCreateInfo indexBufferInfo{};
	indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexBufferInfo.size = indexBufferSize;
	indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer(this->device, &indexBufferInfo, nullptr, &this->quadIndexBuffer);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create quad index buffer: {}", static_cast<int>(result));
		return false;
	}

	/// Allocate index buffer memory
	VkMemoryRequirements indexMemReq;
	vkGetBufferMemoryRequirements(this->device, this->quadIndexBuffer, &indexMemReq);

	VkMemoryAllocateInfo indexAllocInfo{};
	indexAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	indexAllocInfo.allocationSize = indexMemReq.size;
	indexAllocInfo.memoryTypeIndex = this->findMemoryType(
		indexMemReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	result = vkAllocateMemory(this->device, &indexAllocInfo, nullptr, &this->quadIndexMemory);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to allocate quad index memory: {}", static_cast<int>(result));
		return false;
	}

	/// Copy index data to buffer
	void* indexData;
	vkMapMemory(this->device, this->quadIndexMemory, 0, indexBufferSize, 0, &indexData);
	memcpy(indexData, indices.data(), static_cast<size_t>(indexBufferSize));
	vkUnmapMemory(this->device, this->quadIndexMemory);

	vkBindBufferMemory(this->device, this->quadIndexBuffer, this->quadIndexMemory, 0);

	spdlog::debug("Quad geometry created ({} vertices, {} indices)", vertices.size(),
				  indices.size());

	return true;
}

bool MandelbrotDemo::createGraphicsPipeline(VkRenderPass renderPass) {
	/// Step 10: Create Graphics Pipeline and Descriptors
	///
	/// This demonstrates the KEY difference between compute and graphics descriptors:
	/// - Compute uses STORAGE_IMAGE (writeonly, GENERAL layout)
	/// - Graphics uses COMBINED_IMAGE_SAMPLER (readonly, SHADER_READ_ONLY layout)
	///
	/// The same VkImage is used for both, but with different descriptor types and layouts!

	/// Part 1: Create sampler for texture filtering
	/// The sampler controls how the texture is sampled in the fragment shader
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;  /// Bilinear filtering for magnification
	samplerInfo.minFilter = VK_FILTER_LINEAR;  /// Bilinear filtering for minification
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  /// Clamp to edge
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;  /// No anisotropic filtering needed
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;  /// Use normalized [0, 1] coordinates
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	VkResult result = vkCreateSampler(this->device, &samplerInfo, nullptr, &this->sampler);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create sampler: {}", static_cast<int>(result));
		return false;
	}

	/// Part 2: Create descriptor set layout
	/// This defines COMBINED_IMAGE_SAMPLER at set=0, binding=0 for fragment shader
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = 0;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;  /// KEY: not STORAGE_IMAGE
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;  /// Used in fragment shader
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &layoutBinding;

	result = vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr,
										 &this->graphicsDescriptorSetLayout);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create graphics descriptor set layout: {}", static_cast<int>(result));
		return false;
	}

	/// Part 3: Create descriptor pool
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	result = vkCreateDescriptorPool(this->device, &poolInfo, nullptr,
									&this->graphicsDescriptorPool);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create graphics descriptor pool: {}", static_cast<int>(result));
		return false;
	}

	/// Part 4: Allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->graphicsDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &this->graphicsDescriptorSetLayout;

	result = vkAllocateDescriptorSets(this->device, &allocInfo, &this->graphicsDescriptorSet);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to allocate graphics descriptor set: {}", static_cast<int>(result));
		return false;
	}

	/// Part 5: Update descriptor set with image view and sampler
	/// KEY: Image layout is SHADER_READ_ONLY_OPTIMAL (from compute barrier)
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  /// For optimal sampling
	imageInfo.imageView = this->storageImageView;  /// Same view used in compute
	imageInfo.sampler = this->sampler;  /// Sampler for filtering

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = this->graphicsDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(this->device, 1, &descriptorWrite, 0, nullptr);

	/// Part 6: Create pipeline layout
	/// No push constants needed for graphics pipeline
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &this->graphicsDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;  /// No push constants
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr,
									&this->graphicsPipelineLayout);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create graphics pipeline layout: {}", static_cast<int>(result));
		return false;
	}

	/// Part 7: Load shader modules
	std::ifstream vertFile("shaders/fullscreenquad.vert.spv", std::ios::ate | std::ios::binary);
	if (!vertFile.is_open()) {
		spdlog::error("Failed to open vertex shader file");
		return false;
	}

	size_t vertFileSize = static_cast<size_t>(vertFile.tellg());
	std::vector<char> vertCode(vertFileSize);
	vertFile.seekg(0);
	vertFile.read(vertCode.data(), vertFileSize);
	vertFile.close();

	std::ifstream fragFile("shaders/fullscreenquad.frag.spv", std::ios::ate | std::ios::binary);
	if (!fragFile.is_open()) {
		spdlog::error("Failed to open fragment shader file");
		return false;
	}

	size_t fragFileSize = static_cast<size_t>(fragFile.tellg());
	std::vector<char> fragCode(fragFileSize);
	fragFile.seekg(0);
	fragFile.read(fragCode.data(), fragFileSize);
	fragFile.close();

	VkShaderModuleCreateInfo vertModuleInfo{};
	vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertModuleInfo.codeSize = vertCode.size();
	vertModuleInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());

	VkShaderModule vertShaderModule;
	result = vkCreateShaderModule(this->device, &vertModuleInfo, nullptr, &vertShaderModule);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create vertex shader module: {}", static_cast<int>(result));
		return false;
	}

	VkShaderModuleCreateInfo fragModuleInfo{};
	fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragModuleInfo.codeSize = fragCode.size();
	fragModuleInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());

	VkShaderModule fragShaderModule;
	result = vkCreateShaderModule(this->device, &fragModuleInfo, nullptr, &fragShaderModule);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create fragment shader module: {}", static_cast<int>(result));
		vkDestroyShaderModule(this->device, vertShaderModule, nullptr);
		return false;
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	/// Part 8: Create graphics pipeline
	/// This is a simple fullscreen quad pipeline with no depth testing or culling

	/// Vertex input: position (vec2) and texCoord (vec2)
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(QuadVertex);  /// 2 floats + 2 floats = 16 bytes
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
	/// Position at location 0
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;  /// vec2
	attributeDescriptions[0].offset = offsetof(QuadVertex, position);

	/// TexCoord at location 1
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;  /// vec2
	attributeDescriptions[1].offset = offsetof(QuadVertex, texCoord);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	/// Input assembly: triangle list
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	/// Viewport and scissor (dynamic state will be set during render)
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	/// Rasterization: no culling, fill mode
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;  /// No culling for fullscreen quad
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	/// Multisampling: disabled
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/// Depth testing: disabled (fullscreen quad doesn't need depth)
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	/// Color blending: no blending (opaque fractal)
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
										  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	/// Dynamic state: viewport and scissor
	std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
												   VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	/// NOTE: We're creating a pipeline without a render pass for now
	/// This will need to be updated in Step 12 when integrating with Renderer
	/// For now, we'll leave renderPass as VK_NULL_HANDLE
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = this->graphicsPipelineLayout;
	pipelineInfo.renderPass = renderPass;  /// Provided by renderer
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	result = vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
									   &this->graphicsPipeline);

	/// Clean up shader modules (no longer needed after pipeline creation)
	vkDestroyShaderModule(this->device, vertShaderModule, nullptr);
	vkDestroyShaderModule(this->device, fragShaderModule, nullptr);

	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create graphics pipeline: {}", static_cast<int>(result));
		return false;
	}

	spdlog::debug("Graphics pipeline created (COMBINED_IMAGE_SAMPLER, no depth test)");

	return true;
}

uint32_t MandelbrotDemo::findMemoryType(uint32_t typeFilter,
										 VkMemoryPropertyFlags properties) const {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type");
}

void MandelbrotDemo::cleanup() {
	/// Destroy in reverse creation order
	/// Quad buffers (Step 8)
	if (this->quadIndexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(this->device, this->quadIndexBuffer, nullptr);
		this->quadIndexBuffer = VK_NULL_HANDLE;
	}
	if (this->quadIndexMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->quadIndexMemory, nullptr);
		this->quadIndexMemory = VK_NULL_HANDLE;
	}
	if (this->quadVertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(this->device, this->quadVertexBuffer, nullptr);
		this->quadVertexBuffer = VK_NULL_HANDLE;
	}
	if (this->quadVertexMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->quadVertexMemory, nullptr);
		this->quadVertexMemory = VK_NULL_HANDLE;
	}

	/// Graphics pipeline (Step 10)
	if (this->graphicsPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(this->device, this->graphicsPipeline, nullptr);
		this->graphicsPipeline = VK_NULL_HANDLE;
	}

	/// Graphics pipeline layout (Step 10)
	if (this->graphicsPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(this->device, this->graphicsPipelineLayout, nullptr);
		this->graphicsPipelineLayout = VK_NULL_HANDLE;
	}

	/// Graphics descriptor pool (Step 10) - destroys descriptor sets automatically
	if (this->graphicsDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(this->device, this->graphicsDescriptorPool, nullptr);
		this->graphicsDescriptorPool = VK_NULL_HANDLE;
		this->graphicsDescriptorSet = VK_NULL_HANDLE;  /// Implicitly freed
	}

	/// Graphics descriptor layout (Step 10)
	if (this->graphicsDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(this->device, this->graphicsDescriptorSetLayout, nullptr);
		this->graphicsDescriptorSetLayout = VK_NULL_HANDLE;
	}

	/// Sampler (Step 10)
	if (this->sampler != VK_NULL_HANDLE) {
		vkDestroySampler(this->device, this->sampler, nullptr);
		this->sampler = VK_NULL_HANDLE;
	}

	/// Compute pipeline (Step 6)
	if (this->computePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(this->device, this->computePipeline, nullptr);
		this->computePipeline = VK_NULL_HANDLE;
	}

	/// Pipeline layout (Step 6)
	if (this->computePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(this->device, this->computePipelineLayout, nullptr);
		this->computePipelineLayout = VK_NULL_HANDLE;
	}

	/// Descriptor pool (Step 4) - destroys descriptor sets automatically
	if (this->computeDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(this->device, this->computeDescriptorPool, nullptr);
		this->computeDescriptorPool = VK_NULL_HANDLE;
		this->computeDescriptorSet = VK_NULL_HANDLE;  /// Implicitly freed
	}

	/// Descriptor layout (Step 3)
	if (this->computeDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(this->device, this->computeDescriptorSetLayout, nullptr);
		this->computeDescriptorSetLayout = VK_NULL_HANDLE;
	}

	if (this->storageImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(this->device, this->storageImageView, nullptr);
		this->storageImageView = VK_NULL_HANDLE;
	}

	if (this->storageImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->storageImageMemory, nullptr);
		this->storageImageMemory = VK_NULL_HANDLE;
	}

	if (this->storageImage != VK_NULL_HANDLE) {
		vkDestroyImage(this->device, this->storageImage, nullptr);
		this->storageImage = VK_NULL_HANDLE;
	}
}

bool MandelbrotDemo::handleInput(const SDL_Event& event) {
	/// Step 13: Interactive parameter controls
	/// Returns true if parameters changed (requires command buffer re-recording)

	if (event.type != SDL_EVENT_KEY_DOWN) {
		return false;  /// Only handle key presses
	}

	bool paramsChanged = false;

	switch (event.key.key) {
		/// Arrow keys: Pan (adjust offset)
		/// Pan speed is relative to current zoom for intuitive navigation
		case SDLK_LEFT:
			this->params.offset.x -= kPanSpeed * this->params.zoom;
			paramsChanged = true;
			spdlog::debug("Pan left: offset = ({:.3f}, {:.3f})",
						  this->params.offset.x, this->params.offset.y);
			break;

		case SDLK_RIGHT:
			this->params.offset.x += kPanSpeed * this->params.zoom;
			paramsChanged = true;
			spdlog::debug("Pan right: offset = ({:.3f}, {:.3f})",
						  this->params.offset.x, this->params.offset.y);
			break;

		case SDLK_UP:
			this->params.offset.y += kPanSpeed * this->params.zoom;
			paramsChanged = true;
			spdlog::debug("Pan up: offset = ({:.3f}, {:.3f})",
						  this->params.offset.x, this->params.offset.y);
			break;

		case SDLK_DOWN:
			this->params.offset.y -= kPanSpeed * this->params.zoom;
			paramsChanged = true;
			spdlog::debug("Pan down: offset = ({:.3f}, {:.3f})",
						  this->params.offset.x, this->params.offset.y);
			break;

		/// +/= key: Zoom in (divide zoom by kZoomSpeed)
		/// Smaller zoom = more zoomed in
		case SDLK_EQUALS:  /// = key (same as + on US keyboards)
		case SDLK_PLUS:
			this->params.zoom /= kZoomSpeed;
			paramsChanged = true;
			spdlog::info("Zoom in: zoom = {:.4f}", this->params.zoom);
			break;

		/// - key: Zoom out (multiply zoom by kZoomSpeed)
		/// Larger zoom = more zoomed out
		case SDLK_MINUS:
			this->params.zoom *= kZoomSpeed;
			paramsChanged = true;
			spdlog::info("Zoom out: zoom = {:.4f}", this->params.zoom);
			break;

		/// [ key: Decrease iterations
		case SDLK_LEFTBRACKET:
			this->params.maxIter = std::max(32, this->params.maxIter - kIterStep);
			paramsChanged = true;
			spdlog::info("Decrease iterations: maxIter = {}", this->params.maxIter);
			break;

		/// ] key: Increase iterations
		case SDLK_RIGHTBRACKET:
			this->params.maxIter = std::min(2048, this->params.maxIter + kIterStep);
			paramsChanged = true;
			spdlog::info("Increase iterations: maxIter = {}", this->params.maxIter);
			break;

		/// R key: Reset to default view
		case SDLK_R:
			this->params.offset = glm::vec2(0.0f, 0.0f);
			this->params.zoom = 3.0f;
			this->params.maxIter = 256;
			paramsChanged = true;
			spdlog::info("Reset view: offset=(0,0), zoom=3.0, maxIter=256");
			break;

		default:
			/// Not a fractal control key
			break;
	}

	return paramsChanged;
}

} /// namespace lillugsi::rendering
