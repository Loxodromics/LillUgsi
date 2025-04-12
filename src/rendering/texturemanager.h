// texturemanager.h
#pragma once

#include "texture.h"
#include "textureloader.h"
#include "vulkan/commandbuffermanager.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <filesystem>
#include <mutex>

namespace lillugsi::rendering {

/// TextureManager handles the lifecycle and caching of texture resources
/// This centralizes texture management to prevent redundant loading and ensure
/// proper resource sharing and cleanup across the rendering system
class TextureManager {
public:
	/// Create a new texture manager
	/// @param device The logical device for creating GPU resources
	/// @param physicalDevice The physical device for memory allocation
	/// @param commandPool Command pool for transfer operations
	/// @param graphicsQueue Queue for transfer operations
	TextureManager(
		VkDevice device,
		VkPhysicalDevice physicalDevice,
		VkCommandPool commandPool,
		VkQueue graphicsQueue,
		std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager
	);

	/// Destructor ensures proper cleanup of all managed textures
	~TextureManager();

	/// Load a texture from file with caching
	/// If the texture has already been loaded, the existing instance is returned
	/// This prevents redundant loading of the same texture
	///
	/// @param filename Path to the texture file
	/// @param generateMipmaps Whether to generate mipmaps for this texture
	/// @param format Desired format for loading the texture
	/// @return Shared pointer to the loaded texture
	[[nodiscard]] std::shared_ptr<Texture> getOrLoadTexture(
		const std::string& filename,
		bool generateMipmaps = true,
		TextureLoader::Format format = TextureLoader::Format::RGBA
	);

	/// Create a texture from raw pixel data
	/// This is useful for procedurally generated textures or when data comes from
	/// sources other than files (like network or embedded resources)
	///
	/// @param name Unique name to identify this texture (for caching)
	/// @param data Raw pixel data
	/// @param width Width of the texture in pixels
	/// @param height Height of the texture in pixels
	/// @param format Vulkan format for the texture
	/// @param channels Number of color channels in the pixel data
	/// @param generateMipmaps Whether to generate mipmaps for this texture
	/// @return Shared pointer to the created texture
	[[nodiscard]] std::shared_ptr<Texture> createTexture(
		const std::string& name,
		const void* data,
		uint32_t width,
		uint32_t height,
		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
		int channels = 4,
		bool generateMipmaps = true
	);

	/// Check if a texture is already loaded
	/// @param filename Path to the texture file to check
	/// @return True if the texture is already loaded and cached
	[[nodiscard]] bool isTextureLoaded(const std::string& filename) const;

	/// Get a texture by name if it exists
	/// @param name Name of the texture to retrieve
	/// @return Shared pointer to the texture, or nullptr if not found
	[[nodiscard]] std::shared_ptr<Texture> getTexture(const std::string& name) const;

	/// Explicitly release a texture from cache
	/// This can be used to free memory when a texture is no longer needed
	/// Note that the texture will only be destroyed if no other part of the
	/// application holds a reference to it
	///
	/// @param name Name of the texture to release
	/// @return True if the texture was found and released, false otherwise
	bool releaseTexture(const std::string& name);

	/// Release all textures from cache
	/// This is useful during application shutdown or scene transitions
	void releaseAllTextures();

	/// Create a default white texture
	/// This provides a fallback texture when a requested texture is missing
	/// @return Shared pointer to the default white texture
	[[nodiscard]] std::shared_ptr<Texture> createDefaultTexture();

	/// Get the default texture
	/// The default texture is created on-demand if it doesn't exist yet
	/// @return Shared pointer to the default texture
	[[nodiscard]] std::shared_ptr<Texture> getDefaultTexture();

private:
	/// Get a normalized path for consistent texture lookup
	/// This ensures that different path formats pointing to the same file
	/// are treated as the same texture
	///
	/// @param path The path to normalize
	/// @return Normalized path string
	[[nodiscard]] std::string normalizePath(const std::string& path) const;

	VkDevice device;                  /// Logical device reference
	VkPhysicalDevice physicalDevice;  /// Physical device reference
	VkCommandPool commandPool;        /// Command pool for transfer operations
	VkQueue graphicsQueue;            /// Queue for transfer operations

	/// Cache of loaded textures, keyed by normalized path
	/// Using std::unordered_map for O(1) average lookup time
	std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache;

	/// Default texture used as a fallback
	std::shared_ptr<Texture> defaultTexture;

	/// Mutex for thread-safe texture access
	/// This prevents race conditions when multiple threads access the cache
	mutable std::mutex cacheMutex;

	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager;
};

} /// namespace lillugsi::rendering