#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanexception.h"

#include <memory>
#include <string>
#include <vulkan/commandbuffermanager.h>

namespace lillugsi::rendering {

/// Represents a texture image in GPU memory
/// This class manages the lifetime of a texture including its image, memory, view, and sampler
/// It handles all the Vulkan-specific details of texture storage and access
class Texture {
public:
	/// Available filter modes for texture sampling
	/// These determine how texels are interpolated when sampling between pixels
	enum class FilterMode {
		Nearest,    /// No interpolation, uses the nearest texel (pixelated look)
		Linear,     /// Linear interpolation between adjacent texels (smoother look)
		Cubic       /// Higher quality interpolation for better appearance (more expensive)
	};

	/// Available wrapping modes for texture coordinates
	/// These determine how texture coordinates outside [0,1] range are handled
	enum class WrapMode {
		Repeat,         /// Texture repeats (1.2 becomes 0.2)
		MirroredRepeat, /// Texture repeats but mirrored at each integer boundary
		ClampToEdge,    /// Coordinates are clamped to [0,1] (uses edge pixels)
		ClampToBorder   /// Coordinates outside [0,1] use a specified border color
	};

	/// Constructor for creating a texture
	/// @param device The logical device for creating Vulkan resources
	/// @param physicalDevice The physical device for memory allocation
	/// @param width Width of the texture in pixels
	/// @param height Height of the texture in pixels
	/// @param format Pixel format of the texture (e.g., RGBA8)
	/// @param mipLevels Number of mipmap levels (0 for automatic calculation)
	/// @param layerCount Number of array layers (1 for standard 2D texture)
	/// @param name Optional debug name for the texture
	Texture(VkDevice device,
	        VkPhysicalDevice physicalDevice,
	        uint32_t width,
	        uint32_t height,
	        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
	        uint32_t mipLevels = 0,  /// 0 means generate all possible levels
	        uint32_t layerCount = 1,
	        const std::string& name = "");

	/// Destructor ensures proper cleanup of Vulkan resources
	~Texture();

	/// Upload pixel data to the texture
	/// This method transfers pixel data from CPU to GPU memory
	/// It handles creation of staging buffers and proper image transitions
	/// @param data Pointer to the pixel data
	/// @param size Size of the data in bytes
	/// @param commandPool Command pool for allocating transfer commands
	/// @param queue Queue to submit transfer commands to
	/// @param commandBufferManager Command buffer manager to use
	void uploadData(const void* data,
	                size_t size,
	                VkCommandPool commandPool,
	                VkQueue queue,
	                vulkan::CommandBufferManager& commandBufferManager);

	/// Configure the texture's sampler
	/// @param minFilter Filter to use when texture is minified
	/// @param magFilter Filter to use when texture is magnified
	/// @param wrapU Wrap mode for U coordinates
	/// @param wrapV Wrap mode for V coordinates
	/// @param enableAnisotropy Whether to enable anisotropic filtering
	/// @param maxAnisotropy Maximum anisotropy level (typically 16.0f)
	void configureSampler(FilterMode minFilter = FilterMode::Linear,
	                      FilterMode magFilter = FilterMode::Linear,
	                      WrapMode wrapU = WrapMode::Repeat,
	                      WrapMode wrapV = WrapMode::Repeat,
	                      bool enableAnisotropy = true,
	                      float maxAnisotropy = 16.0f);

	/// Generate mipmaps for the texture
	/// This is needed for textures that didn't have mipmaps pre-generated
	/// @param commandPool Command pool for allocating barrier commands
	/// @param queue Queue to submit commands to
	/// @param commandBufferManager
	void generateMipmaps(
		VkCommandPool commandPool,
		VkQueue queue,
		vulkan::CommandBufferManager &commandBufferManager);

	/// Get the texture's image view
	/// This is needed for binding the texture to descriptors
	/// @return Handle to the image view
	[[nodiscard]] VkImageView getImageView() const { return this->imageView.get(); }

	/// Get the texture's sampler
	/// This is needed for binding the texture to descriptors
	/// @return Handle to the sampler
	[[nodiscard]] VkSampler getSampler() const { return this->sampler.get(); }

	/// Get the width of the texture
	/// @return Width in pixels
	[[nodiscard]] uint32_t getWidth() const { return this->width; }

	/// Get the height of the texture
	/// @return Height in pixels
	[[nodiscard]] uint32_t getHeight() const { return this->height; }

	/// Get the format of the texture
	/// @return Pixel format
	[[nodiscard]] VkFormat getFormat() const { return this->format; }

	/// Get the number of mipmap levels
	/// @return Mipmap level count
	[[nodiscard]] uint32_t getMipLevels() const { return this->mipLevels; }

	/// Check if the texture has an alpha channel
	/// This is useful for determining if alpha blending is needed
	/// @return True if the texture format contains alpha
	[[nodiscard]] bool hasAlpha() const;

	/// Get the debug name of this texture (if any)
	/// @return The name of the texture
	[[nodiscard]] const std::string& getName() const { return this->name; }

private:
	/// Transition the image layout
	/// This handles the proper image memory barriers for layout transitions
	/// @param commandBufferManager
	/// @param commandPool Command pool for allocating barrier commands
	/// @param queue Queue to submit commands to
	/// @param oldLayout Current layout of the image
	/// @param newLayout Desired layout of the image
	/// @param baseMipLevel Base mipmap level for the transition
	/// @param levelCount Number of mipmap levels to transition
	/// @param baseArrayLayer Base array layer for the transition
	/// @param layerCount Number of array layers to transition
	void transitionLayout(
		vulkan::CommandBufferManager& commandBufferManager,
		VkCommandPool commandPool,
		VkQueue queue,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		uint32_t baseMipLevel = 0,
		uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
		uint32_t baseArrayLayer = 0,
		uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS);

	/// Begin single-time commands using the command buffer manager
	/// @param commandPool Command pool to allocate from
	/// @param commandBufferManager Command buffer manager to use
	/// @return Command buffer ready for recording
	[[nodiscard]] VkCommandBuffer beginSingleTimeCommands(
		VkCommandPool commandPool,
		vulkan::CommandBufferManager& commandBufferManager) const
	{
		return commandBufferManager.beginSingleTimeCommands(commandPool);
	}

	/// End and submit single-time commands using the command buffer manager
	/// @param commandBuffer Command buffer to submit
	/// @param commandPool Command pool the buffer was allocated from
	/// @param queue Queue to submit to
	/// @param commandBufferManager Command buffer manager to use
	void endSingleTimeCommands(
		VkCommandBuffer commandBuffer,
		VkCommandPool commandPool,
		VkQueue queue,
		vulkan::CommandBufferManager& commandBufferManager)
	{
		commandBufferManager.endSingleTimeCommands(commandBuffer, commandPool, queue);
	}

	/// Calculate the number of mipmap levels for the texture
	/// Based on the texture dimensions to determine maximum possible levels
	/// @param width Texture width
	/// @param height Texture height
	/// @return Number of mipmap levels
	[[nodiscard]] static uint32_t calculateMipLevels(uint32_t width, uint32_t height);

	/// Find a supported memory type for the texture
	/// @param typeFilter Bit field of suitable memory types
	/// @param properties Desired memory properties
	/// @return Index of suitable memory type
	[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter,
	                                      VkMemoryPropertyFlags properties) const;

	/// Check if the device supports the requested format with optimal tiling
	/// @param format The format to check
	/// @param featureFlags The features required (e.g., for sampling)
	/// @return True if the format is supported
	[[nodiscard]] bool isFormatSupported(VkFormat format,
	                                    VkFormatFeatureFlags featureFlags) const;

	/// Convert filter mode enum to Vulkan filter type
	/// @param mode The filter mode
	/// @return Equivalent Vulkan filter
	[[nodiscard]] static VkFilter toVkFilter(FilterMode mode);

	/// Convert wrap mode enum to Vulkan address mode
	/// @param mode The wrap mode
	/// @return Equivalent Vulkan address mode
	[[nodiscard]] static VkSamplerAddressMode toVkAddressMode(WrapMode mode);

	VkDevice device;                         /// Logical device reference
	VkPhysicalDevice physicalDevice;         /// Physical device reference

	uint32_t width;                          /// Texture width in pixels
	uint32_t height;                         /// Texture height in pixels
	uint32_t mipLevels;                      /// Number of mipmap levels
	uint32_t layerCount;                     /// Number of array layers
	VkFormat format;                         /// Pixel format

	std::string name;                        /// Debug name for the texture

	vulkan::VulkanImageHandle image;         /// Handle to the Vulkan image
	VkDeviceMemory imageMemory{VK_NULL_HANDLE}; /// Memory allocated for the image
	vulkan::VulkanImageViewHandle imageView;  /// Handle to the image view
	vulkan::VulkanSamplerHandle sampler;      /// Handle to the sampler

	VkImageLayout currentLayout{VK_IMAGE_LAYOUT_UNDEFINED}; /// Current layout of the image
	bool hasSampler{false};                  /// Whether a sampler has been created
	bool mipmapsGenerated{false};            /// Whether mipmaps have been generated
};

} /// namespace lillugsi::rendering