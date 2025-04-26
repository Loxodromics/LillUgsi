#include "textureloadingpipeline.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>

namespace lillugsi::rendering {

TextureLoadingPipeline::TextureLoadingPipeline(std::shared_ptr<TextureManager> textureManager)
	: textureManager(std::move(textureManager)) {
	
	/// Set some reasonable defaults for texture loading
	this->defaultOptions.generateMipmaps = true;
	this->defaultOptions.useAnisotropicFiltering = true;
	this->defaultOptions.anisotropyLevel = 16.0f;
	this->defaultOptions.cacheTextures = true;
	this->defaultOptions.convertSRGBToLinear = false;
	
	spdlog::info("Texture loading pipeline created");
}

TextureLoadingPipeline::~TextureLoadingPipeline() {
	/// Wait for all pending operations before destruction
	/// This prevents crashes from callbacks after destruction
	this->waitForAll();
	
	/// Clear the cache to release any remaining references
	std::lock_guard<std::mutex> cacheLock(this->cacheMutex);
	this->textureCache.clear();
	
	spdlog::info("Texture loading pipeline destroyed");
}

void TextureLoadingPipeline::setBaseDirectory(const std::string& directory) {
	this->baseDirectory = directory;
	
	/// Ensure the directory has a trailing separator if it's not empty
	if (!this->baseDirectory.empty() && 
		this->baseDirectory.back() != '/' && 
		this->baseDirectory.back() != '\\') {
		this->baseDirectory += '/';
	}
	
	spdlog::debug("Texture base directory set to: {}", this->baseDirectory);
}

const std::string& TextureLoadingPipeline::getBaseDirectory() const {
	return this->baseDirectory;
}

std::shared_future<std::shared_ptr<Texture>> TextureLoadingPipeline::requestTextureAsync(
	const std::string& texturePath,
	TextureLoader::Format format,
	const TextureLoadOptions& options) {

	/// Process any completed operations to keep the queue clean
	this->processCompletedOperations();

	/// Resolve and normalize the texture path for consistent lookup
	std::string resolvedPath = this->resolveTexturePath(texturePath);

	/// Check if this texture is already loading
	{
		std::lock_guard<std::mutex> lock(this->operationsMutex);
		for (const auto& op : this->asyncOperations) {
			if (op.texturePath == resolvedPath) {
				spdlog::debug("Texture '{}' is already loading, returning existing future", resolvedPath);
				return op.future;
			}
		}
	}

	/// Check if the texture is already in the cache
	/// This avoids unnecessary loading for textures we already have
	std::shared_ptr<Texture> cachedTexture = this->checkCache(resolvedPath);
	if (cachedTexture) {
		/// Create a future that's already satisfied with the cached texture
		std::promise<std::shared_ptr<Texture>> promise;
		promise.set_value(cachedTexture);
		spdlog::debug("Returning cached texture for '{}'", resolvedPath);
		return promise.get_future().share();
	}

	/// Start a new asynchronous loading task
	/// We use std::async with launch::async to ensure it starts in a new thread
	auto future = std::async(std::launch::async, [this, resolvedPath, format, options]() {
		return this->loadTextureInternal(resolvedPath, format, options);
	});

	/// Create a shared future to allow multiple waiters
	/// This is important for textures used by multiple materials
	std::shared_future<std::shared_ptr<Texture>> sharedFuture = future.share();

	/// Track this operation
	{
		std::lock_guard<std::mutex> lock(this->operationsMutex);
		AsyncTextureOperation operation;
		operation.texturePath = resolvedPath;
		operation.format = format;
		operation.options = options;
		operation.future = sharedFuture;
		this->asyncOperations.push_back(std::move(operation));
	}

	spdlog::debug("Queued async load for texture '{}' (total pending: {})",
		resolvedPath, this->getPendingOperationCount());

	return sharedFuture;
}

std::shared_ptr<Texture> TextureLoadingPipeline::loadTexture(
	const std::string& texturePath,
	TextureLoader::Format format,
	const TextureLoadOptions& options) {

	/// Resolve the texture path
	std::string resolvedPath = this->resolveTexturePath(texturePath);

	/// Check if the texture is already in the cache
	std::shared_ptr<Texture> cachedTexture = this->checkCache(resolvedPath);
	if (cachedTexture) {
		spdlog::debug("Using cached texture for '{}'", resolvedPath);
		return cachedTexture;
	}

	/// Check if the texture is currently loading asynchronously
	/// If so, we'll wait for it rather than starting a new load
	std::shared_future<std::shared_ptr<Texture>> pendingFuture;
	bool isLoading = false;

	{
		std::lock_guard<std::mutex> lock(this->operationsMutex);
		for (const auto& op : this->asyncOperations) {
			if (op.texturePath == resolvedPath) {
				pendingFuture = op.future;
				isLoading = true;
				break;
			}
		}
	}

	if (isLoading) {
		/// Wait for the existing async operation to complete
		spdlog::debug("Waiting for async texture '{}' to complete", resolvedPath);
		return pendingFuture.get();
	}

	/// Load the texture synchronously
	spdlog::debug("Loading texture '{}' synchronously", resolvedPath);
	return this->loadTextureInternal(resolvedPath, format, options);
}

bool TextureLoadingPipeline::isTextureLoading(const std::string& texturePath) const {
	/// Resolve the texture path
	std::string resolvedPath = this->resolveTexturePath(texturePath);

	std::lock_guard<std::mutex> lock(this->operationsMutex);
	for (const auto& op : this->asyncOperations) {
		if (op.texturePath == resolvedPath) {
			/// Check if it's still loading (not ready)
			auto status = op.future.wait_for(std::chrono::milliseconds(0));
			return status != std::future_status::ready;
		}
	}

	return false;
}

void TextureLoadingPipeline::waitForAll() {
	/// Make a copy of all operations to wait for
	std::vector<std::shared_future<std::shared_ptr<Texture>>> futures;

	{
		std::lock_guard<std::mutex> lock(this->operationsMutex);
		for (const auto& op : this->asyncOperations) {
			futures.push_back(op.future);
		}
	}

	if (!futures.empty()) {
		spdlog::info("Waiting for {} texture loading operations to complete", futures.size());

		/// Wait for each operation to complete
		for (auto& future : futures) {
			future.wait();
		}

		/// Process completed operations to clean up
		this->processCompletedOperations();

		spdlog::info("All texture loading operations completed");
	}
}

size_t TextureLoadingPipeline::processCompletedOperations() {
	std::lock_guard<std::mutex> lock(this->operationsMutex);

	/// Get the current size for return value
	size_t startSize = this->asyncOperations.size();

	/// Remove completed operations
	this->asyncOperations.erase(
		std::remove_if(this->asyncOperations.begin(), this->asyncOperations.end(),
			[](const AsyncTextureOperation& op) {
				auto status = op.future.wait_for(std::chrono::milliseconds(0));
				return status == std::future_status::ready;
			}),
		this->asyncOperations.end());

	/// Calculate how many were removed
	size_t processedCount = startSize - this->asyncOperations.size();

	if (processedCount > 0) {
		spdlog::debug("Processed {} completed texture operations, {} remaining",
			processedCount, this->asyncOperations.size());
	}

	return processedCount;
}

void TextureLoadingPipeline::setDefaultOptions(const TextureLoadOptions& options) {
	// Create a new copy of the options instead of using assignment
	this->defaultOptions = TextureLoadOptions{
		options.generateMipmaps,
		options.useAnisotropicFiltering,
		options.convertSRGBToLinear,
		options.cacheTextures,
		options.anisotropyLevel
	};

	spdlog::debug("Set default texture loading options: mipmaps={}, anisotropic={}",
		options.generateMipmaps, options.useAnisotropicFiltering);
}

const TextureLoadOptions& TextureLoadingPipeline::getDefaultOptions() const {
	return this->defaultOptions;
}
size_t TextureLoadingPipeline::getPendingOperationCount() const {
	std::lock_guard<std::mutex> lock(this->operationsMutex);
	return this->asyncOperations.size();
}

std::string TextureLoadingPipeline::resolveTexturePath(const std::string& texturePath) const {
	/// Handle empty paths
	if (texturePath.empty()) {
		return "";
	}

	try {
		/// Create a filesystem path object
		std::filesystem::path path(texturePath);

		/// If it's already absolute, normalize and return
		if (path.is_absolute()) {
			return path.lexically_normal().string();
		}

		/// If we have a base directory, combine them
		if (!this->baseDirectory.empty()) {
			std::filesystem::path result = std::filesystem::path(this->baseDirectory) / path;
			return result.lexically_normal().string();
		}

		/// No base directory, normalize the relative path
		return path.lexically_normal().string();
	}
	catch (const std::exception& e) {
		/// Handle filesystem exceptions, fallback to simple concatenation
		spdlog::warn("Path resolution error for '{}': {}", texturePath, e.what());

		if (!this->baseDirectory.empty() &&
			texturePath.find(":/") == std::string::npos &&
			texturePath.find(":\\") == std::string::npos) {
			return this->baseDirectory + texturePath;
		}

		return texturePath;
	}
}

std::shared_ptr<Texture> TextureLoadingPipeline::loadTextureInternal(
	const std::string& texturePath,
	TextureLoader::Format format,
	const TextureLoadOptions& options) {

	/// Use the provided options or fall back to defaults
	const TextureLoadOptions& effectiveOptions =
		(options.cacheTextures == this->defaultOptions.cacheTextures &&
		 options.generateMipmaps == this->defaultOptions.generateMipmaps &&
		 options.useAnisotropicFiltering == this->defaultOptions.useAnisotropicFiltering &&
		 options.anisotropyLevel == this->defaultOptions.anisotropyLevel &&
		 options.convertSRGBToLinear == this->defaultOptions.convertSRGBToLinear)
		? this->defaultOptions
		: options;

	try {
		/// Log the start of texture loading
		spdlog::debug("Loading texture '{}' with format {}", texturePath, static_cast<int>(format));

		/// Load the texture using the texture manager
		auto texture = this->textureManager->getOrLoadTexture(
			texturePath,
			effectiveOptions.generateMipmaps,
			format
		);

		/// Handle case where texture failed to load
		if (!texture) {
			std::string error = this->formatErrorMessage(texturePath, "Failed to load");
			spdlog::error("{}", error);

			/// Return the default texture as a fallback
			/// This ensures materials still render even if textures are missing
			spdlog::debug("Using default texture as fallback for '{}'", texturePath);
			return this->textureManager->getDefaultTexture();
		}

		/// Configure texture parameters based on options

		/// Determine the appropriate filter mode
		Texture::FilterMode filterMode = effectiveOptions.useAnisotropicFiltering
			? Texture::FilterMode::Linear  /// Anisotropic uses linear as base
			: Texture::FilterMode::Linear; /// Default to linear for best quality

		/// Configure the texture sampler with the requested options
		texture->configureSampler(
			filterMode,                          /// Min filter
			filterMode,                          /// Mag filter
			Texture::WrapMode::Repeat,           /// Wrap U - repeat for tiling
			Texture::WrapMode::Repeat,           /// Wrap V - repeat for tiling
			effectiveOptions.useAnisotropicFiltering, /// Enable anisotropic filtering
			effectiveOptions.anisotropyLevel     /// Anisotropy level
		);

		/// Cache the texture if requested
		if (effectiveOptions.cacheTextures) {
			this->addToCache(texturePath, texture);
		}

		spdlog::info("Successfully loaded texture '{}' ({}x{})",
			texturePath, texture->getWidth(), texture->getHeight());

		return texture;
	}
	catch (const vulkan::VulkanException& e) {
		/// Handle Vulkan errors that might occur during texture loading
		std::string error = this->formatErrorMessage(texturePath, e.what());
		spdlog::error("{}", error);
		return this->textureManager->getDefaultTexture();
	}
	catch (const std::exception& e) {
		/// Handle any other unexpected errors
		std::string error = this->formatErrorMessage(texturePath, e.what());
		spdlog::error("{}", error);
		return this->textureManager->getDefaultTexture();
	}
}

std::shared_ptr<Texture> TextureLoadingPipeline::checkCache(const std::string& texturePath) const {
	/// Only check cache if we're using it
	if (!this->defaultOptions.cacheTextures) {
		return nullptr;
	}

	std::lock_guard<std::mutex> lock(this->cacheMutex);

	/// Look up in our cache
	auto it = this->textureCache.find(texturePath);
	if (it != this->textureCache.end()) {
		/// Check if the weak pointer is still valid
		auto texture = it->second.lock();
		if (texture) {
			spdlog::trace("Cache hit for texture '{}'", texturePath);
			return texture;
		}

		/// Weak pointer expired, remove from cache
		/// Note: We need to cast away constness here because we're modifying the cache in a const method
		/// This is logically const since it doesn't change the observable behavior
		spdlog::trace("Removing expired texture from cache: '{}'", texturePath);
		const_cast<std::unordered_map<std::string, std::weak_ptr<Texture>>&>(this->textureCache).erase(it);
	}
	
	/// Check if the texture manager already has it
	/// This handles textures loaded outside the pipeline
	if (this->textureManager->isTextureLoaded(texturePath)) {
		auto texture = this->textureManager->getTexture(texturePath);
		if (texture) {
			spdlog::trace("Found texture in TextureManager: '{}'", texturePath);
			
			/// Update our cache with this texture
			const_cast<TextureLoadingPipeline*>(this)->addToCache(texturePath, texture);
			
			return texture;
		}
	}
	
	return nullptr;
}

void TextureLoadingPipeline::addToCache(const std::string& texturePath, std::shared_ptr<Texture> texture) {
	/// Only cache if enabled
	if (!this->defaultOptions.cacheTextures) {
		return;
	}
	
	std::lock_guard<std::mutex> lock(this->cacheMutex);
	
	/// Store weak_ptr to allow for proper resource cleanup
	/// This prevents the cache from keeping textures alive when they're no longer used
	this->textureCache[texturePath] = texture;
	
	spdlog::trace("Added texture '{}' to cache", texturePath);
}

std::string TextureLoadingPipeline::formatErrorMessage(
	const std::string& texturePath, 
	const std::string& reason) const {
	
	/// Create a detailed error message with both the path and the reason
	/// This helps with debugging texture loading issues
	return "Error loading texture '" + texturePath + "': " + reason;
}

} /// namespace lillugsi::rendering