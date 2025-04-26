#pragma once

#include "rendering/texturemanager.h"
#include "rendering/texture.h"
#include <memory>
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <unordered_map>

namespace lillugsi::rendering {

/// Configuration options for texture loading
/// This struct defines customization options for the texture loading process
struct TextureLoadOptions {
    bool generateMipmaps = true;         /// Whether to generate mipmaps for loaded textures
    bool useAnisotropicFiltering = true; /// Whether to enable anisotropic filtering
    bool convertSRGBToLinear = false;    /// Whether to convert sRGB textures to linear space
    bool cacheTextures = true;           /// Whether to cache textures for reuse
    float anisotropyLevel = 16.0f;       /// Maximum anisotropy level for filtering
};

/// TextureLoadingPipeline manages the asynchronous loading and processing of model textures
/// We separate texture loading from model loading to:
/// 1. Allow textures to load in parallel without blocking model construction
/// 2. Provide consistent texture format handling for different model formats
/// 3. Enable texture caching and reuse across multiple models
/// 4. Support advanced texture processing like mipmap generation and format conversion
class TextureLoadingPipeline {
public:
	/// Create a texture loading pipeline
	/// @param textureManager The texture manager to use for texture loading and storage
	explicit TextureLoadingPipeline(std::shared_ptr<TextureManager> textureManager);

	/// Destroy the texture loading pipeline, canceling any pending operations
	~TextureLoadingPipeline();

	/// Set the base directory for texture resolution
	/// This is used when resolving relative texture paths
	/// @param directory The base directory path
	void setBaseDirectory(const std::string& directory);

	/// Get the base directory for texture resolution
	/// @return The current base directory
	[[nodiscard]] const std::string& getBaseDirectory() const;

	/// Request a texture to be loaded asynchronously
	/// This queues the texture for loading in a background thread
	/// @param texturePath Path to the texture file
	/// @param format The desired texture format
	/// @param options Options for texture loading and processing
	/// @return A future that will contain the loaded texture
	[[nodiscard]] std::shared_future<std::shared_ptr<Texture>> requestTextureAsync(
		const std::string& texturePath,
		TextureLoader::Format format = TextureLoader::Format::RGBA,
		const TextureLoadOptions& options = TextureLoadOptions());

	/// Load a texture synchronously
	/// This blocks until the texture is loaded
	/// @param texturePath Path to the texture file
	/// @param format The desired texture format
	/// @param options Options for texture loading and processing
	/// @return The loaded texture, or nullptr if loading failed
	[[nodiscard]] std::shared_ptr<Texture> loadTexture(
		const std::string& texturePath,
		TextureLoader::Format format = TextureLoader::Format::RGBA,
		const TextureLoadOptions& options = TextureLoadOptions());

	/// Check if a texture is currently loading
	/// @param texturePath Path to the texture to check
	/// @return True if the texture is being loaded asynchronously
	[[nodiscard]] bool isTextureLoading(const std::string& texturePath) const;

	/// Wait for all pending texture loads to complete
	/// This blocks until all requested textures have finished loading
	void waitForAll();

	/// Process any completed async operations
	/// This should be called periodically to clean up finished tasks
	/// @return Number of completed operations processed
	size_t processCompletedOperations();

	/// Set global defaults for texture loading
	/// These options are used when specific options aren't provided
	/// @param options The default options to use
	void setDefaultOptions(const TextureLoadOptions& options);

	/// Get the current default options
	/// @return The current default texture loading options
	[[nodiscard]] const TextureLoadOptions& getDefaultOptions() const;

	/// Get the number of currently pending texture operations
	/// @return Count of textures currently being loaded
	[[nodiscard]] size_t getPendingOperationCount() const;

private:
	/// Resolve a texture path against the base directory
	/// This handles both absolute and relative paths
	/// @param texturePath The texture path to resolve
	/// @return The resolved absolute path
	[[nodiscard]] std::string resolveTexturePath(const std::string& texturePath) const;

	/// Load a texture with specific options
	/// This is the internal implementation used by both sync and async methods
	/// @param texturePath Path to the texture file
	/// @param format The desired texture format
	/// @param options Options for texture loading and processing
	/// @return The loaded texture, or nullptr if loading failed
	[[nodiscard]] std::shared_ptr<Texture> loadTextureInternal(
		const std::string& texturePath,
		TextureLoader::Format format,
		const TextureLoadOptions& options);

	/// Check the texture cache for an existing texture
	/// @param texturePath The normalized texture path to look up
	/// @return The cached texture, or nullptr if not found
	[[nodiscard]] std::shared_ptr<Texture> checkCache(const std::string& texturePath) const;

	/// Add a texture to the cache
	/// @param texturePath The normalized texture path
	/// @param texture The texture to cache
	void addToCache(const std::string& texturePath, std::shared_ptr<Texture> texture);

	/// Information about an asynchronous texture loading operation
	struct AsyncTextureOperation {
		std::string texturePath;       /// Path of the texture being loaded
		TextureLoader::Format format;  /// Requested format
		TextureLoadOptions options;    /// Loading options
		std::shared_future<std::shared_ptr<Texture>> future; /// Future for the result
	};

	std::shared_ptr<TextureManager> textureManager; /// Texture manager for loading
	std::string baseDirectory;                      /// Base directory for paths
	TextureLoadOptions defaultOptions;              /// Default loading options
	
	/// Cache of loaded textures for quick lookup
	/// Maps normalized paths to loaded textures
	mutable std::mutex cacheMutex;
	std::unordered_map<std::string, std::weak_ptr<Texture>> textureCache;
	
	/// Currently running async operations
	mutable std::mutex operationsMutex;
	std::vector<AsyncTextureOperation> asyncOperations;
	
	/// Format the error message for texture loading failures
	/// @param texturePath The path that failed to load
	/// @param reason The reason for the failure
	/// @return Formatted error message
	[[nodiscard]] std::string formatErrorMessage(
		const std::string& texturePath, 
		const std::string& reason) const;
};

} /// namespace lillugsi::rendering