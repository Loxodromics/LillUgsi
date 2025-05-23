#include "texturemanager.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <utility>

namespace lillugsi::rendering {

TextureManager::TextureManager(
	VkDevice device,
	VkPhysicalDevice physicalDevice,
	VkCommandPool commandPool,
	VkQueue graphicsQueue,
	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager
) : device(device)
  , physicalDevice(physicalDevice)
  , commandPool(commandPool)
  , graphicsQueue(graphicsQueue)
  , defaultTexture(nullptr)
  , commandBufferManager(std::move(commandBufferManager)){

	spdlog::info("Texture manager initialized");
}

TextureManager::~TextureManager() {
	this->releaseAllTextures();
	spdlog::info("Texture manager destroyed");
}

std::shared_ptr<Texture> TextureManager::getOrLoadTexture(
	const std::string& filename,
	bool generateMipmaps,
	TextureLoader::Format format
) {
	/// Normalize the path for consistent lookup
	std::string normalizedPath = this->normalizePath(filename);

	/// First, check if texture is already loaded
	/// This avoids costly file I/O and GPU uploads for textures we already have
	{
		/// Use a scoped lock to ensure thread safety during cache lookup
		std::lock_guard<std::mutex> lock(this->cacheMutex);

		auto it = this->textureCache.find(normalizedPath);
		if (it != this->textureCache.end()) {
			/// Return cached texture if found
			spdlog::debug("Using cached texture: {}", normalizedPath);
			return it->second;
		}
	}

	/// Texture not found in cache, need to load it
	spdlog::debug("Loading texture: {}", normalizedPath);

	/// Use TextureLoader to load the pixel data from file
	auto textureData = TextureLoader::loadFromFile(normalizedPath, format);

	/// If loading failed, return the default texture as a fallback
	if (!textureData.success) {
		spdlog::warn("Failed to load texture '{}': {}", normalizedPath, textureData.errorMessage);
		return this->getDefaultTexture();
	}

	/// Determine appropriate Vulkan format based on channels
	/// This mapping ensures we use the right format for the loaded pixel data
	VkFormat vulkanFormat;
	if (format == TextureLoader::Format::NormalMap) {
		/// Always use unorm format for normal maps regardless of channel count
		/// And ensure we have RGBA data
		if (textureData.channels == 3) {
			/// Convert RGB to RGBA by adding alpha channel
			spdlog::debug("Converting RGB to RGBA for normal map");
			std::vector<uint8_t> rgbaData(textureData.width * textureData.height * 4);
			for (int i = 0; i < textureData.width * textureData.height; i++) {
				rgbaData[i * 4 + 0] = textureData.pixels[i * 3 + 0]; /// R
				rgbaData[i * 4 + 1] = textureData.pixels[i * 3 + 1]; /// G
				rgbaData[i * 4 + 2] = textureData.pixels[i * 3 + 2]; /// B
				rgbaData[i * 4 + 3] = 255;                           /// A (fully opaque)
			}
			/// Swap the pixel data
			textureData.pixels = std::move(rgbaData);
			textureData.channels = 4;
		}
		/// By using VK_FORMAT_R8G8B8A8_UNORM, we ensure the values are read exactly as they are
		/// stored, without any gamma correction. This preserves the linear relationship needed for
		/// proper normal vector calculations.
		vulkanFormat = VK_FORMAT_R8G8B8A8_UNORM; /// Linear color space for normal maps
	} else {
		switch (textureData.channels) {
		case 1:
			vulkanFormat = VK_FORMAT_R8_UNORM;
			break;
		case 2:
			vulkanFormat = VK_FORMAT_R8G8_UNORM;
			break;
		case 3: {
			// Convert RGB to RGBA for better compatibility
			spdlog::debug("Converting RGB to RGBA for better compatibility");
			std::vector<uint8_t> rgbaData(textureData.width * textureData.height * 4);
			for (int i = 0; i < textureData.width * textureData.height; i++) {
				rgbaData[i * 4 + 0] = textureData.pixels[i * 3 + 0]; /// R
				rgbaData[i * 4 + 1] = textureData.pixels[i * 3 + 1]; /// G
				rgbaData[i * 4 + 2] = textureData.pixels[i * 3 + 2]; /// B
				rgbaData[i * 4 + 3] = 255;                           /// A (fully opaque)
			}
			/// Swap the pixel data
			textureData.pixels = std::move(rgbaData);
			textureData.channels = 4;
			vulkanFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		}
		case 4:
			vulkanFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		default:
			spdlog::warn("Unsupported channel count {}, falling back to RGBA", textureData.channels);
			vulkanFormat = VK_FORMAT_R8G8B8A8_SRGB;
		}
	}

	/// Create mipmap levels based on settings and dimensions
	uint32_t mipLevels = generateMipmaps ? 0 : 1; /// 0 means calculate automatically

	/// Extract filename without path for debugging and identification
	std::filesystem::path path(normalizedPath);
	std::string name = path.filename().string();

	/// Create the texture with the loaded data
	try {
		/// Create the GPU texture resource
		auto texture = std::make_shared<Texture>(
			this->device,
			this->physicalDevice,
			static_cast<uint32_t>(textureData.width),
			static_cast<uint32_t>(textureData.height),
			vulkanFormat,
			mipLevels,
			1, // Single layer for standard textures
			name // Use filename as texture name
		);

		/// Upload the pixel data to the GPU
		texture->uploadData(
			textureData.pixels.data(),
			textureData.pixels.size(),
			this->commandPool,
			this->graphicsQueue,
			*this->commandBufferManager.get()
		);

		/// Configure default sampler settings
		/// These settings work well for most use cases
		texture->configureSampler(
			Texture::FilterMode::Linear,
			Texture::FilterMode::Linear,
			Texture::WrapMode::Repeat,
			Texture::WrapMode::Repeat,
			true, // Enable anisotropic filtering
			16.0f // Max anisotropy
		);

		/// Cache the texture for future use
		{
			std::lock_guard<std::mutex> lock(this->cacheMutex);
			this->textureCache[normalizedPath] = texture;
		}

		spdlog::info("Loaded and cached texture: {}", normalizedPath);
		return texture;
	}
	catch (const vulkan::VulkanException& e) {
		/// Handle Vulkan errors during texture creation
		spdlog::error("Failed to create texture '{}': {}", normalizedPath, e.what());
		return this->getDefaultTexture();
	}
	catch (const std::exception& e) {
		/// Handle any other errors
		spdlog::error("Unexpected error creating texture '{}': {}", normalizedPath, e.what());
		return this->getDefaultTexture();
	}
}

std::shared_ptr<Texture> TextureManager::createTexture(
	const std::string& name,
	const void* data,
	uint32_t width,
	uint32_t height,
	VkFormat format,
	int channels,
	bool generateMipmaps
) {
	/// Early check for invalid input
	if (!data || width == 0 || height == 0) {
		spdlog::error("Invalid texture data for texture '{}'", name);
		return this->getDefaultTexture();
	}

	/// First, check if a texture with this name already exists
	{
		std::lock_guard<std::mutex> lock(this->cacheMutex);

		auto it = this->textureCache.find(name);
		if (it != this->textureCache.end()) {
			spdlog::warn("Texture '{}' already exists, returning existing texture", name);
			return it->second;
		}
	}

	/// Calculate appropriate mip levels
	uint32_t mipLevels = generateMipmaps ? 0 : 1; // 0 means calculate automatically

	/// Calculate data size based on dimensions and channels
	size_t dataSize = width * height * channels;

	try {
		/// Create the GPU texture resource
		auto texture = std::make_shared<Texture>(
			this->device,
			this->physicalDevice,
			width,
			height,
			format,
			mipLevels,
			1, /// Single layer
			name
		);

		/// Upload the pixel data to the GPU
		texture->uploadData(
			data,
			dataSize,
			this->commandPool,
			this->graphicsQueue,
			*this->commandBufferManager.get()
		);

		/// Configure default sampler settings
		texture->configureSampler(
			Texture::FilterMode::Linear,
			Texture::FilterMode::Linear,
			Texture::WrapMode::Repeat,
			Texture::WrapMode::Repeat,
			true,
			16.0f
		);

		/// Cache the texture
		{
			std::lock_guard<std::mutex> lock(this->cacheMutex);
			this->textureCache[name] = texture;
		}

		spdlog::info("Created and cached texture: {}, {}x{}", name, width, height);
		return texture;
	}
	catch (const vulkan::VulkanException& e) {
		spdlog::error("Failed to create texture '{}': {}", name, e.what());
		return this->getDefaultTexture();
	}
	catch (const std::exception& e) {
		spdlog::error("Unexpected error creating texture '{}': {}", name, e.what());
		return this->getDefaultTexture();
	}
}

std::shared_ptr<Texture> TextureManager::createTextureFromBufferView(
	const std::string& name,
	const void* bufferData,
	size_t bufferSize,
	const std::string& mimeType,
	bool generateMipmaps,
	TextureLoader::Format format
) {
	/// Early check for invalid input
	if (!bufferData || bufferSize == 0) {
		spdlog::error("Invalid buffer data for texture '{}'", name);
		return this->getDefaultTexture();
	}

	/// First, check if a texture with this name already exists
	/// This allows us to avoid redundant processing of the same embedded texture
	{
		std::lock_guard<std::mutex> lock(this->cacheMutex);

		auto it = this->textureCache.find(name);
		if (it != this->textureCache.end()) {
			spdlog::debug("Embedded texture '{}' already exists, returning cached version", name);
			return it->second;
		}
	}

	/// Use the TextureLoader to decode the buffer data into pixel data
	/// This handles different image formats embedded in the glTF file
	auto textureData = TextureLoader::loadFromBufferView(
		bufferData,
		bufferSize,
		mimeType,
		format
	);

	/// If loading failed, return the default texture as a fallback
	if (!textureData.success) {
		spdlog::warn("Failed to load embedded texture '{}': {}", name, textureData.errorMessage);
		return this->getDefaultTexture();
	}

	/// Determine the appropriate Vulkan format based on the loaded data
	/// This mapping is similar to the file loading path but specifically
	/// handles embedded texture data which might have different characteristics
	VkFormat vulkanFormat;
	if (format == TextureLoader::Format::NormalMap) {
		/// Normal maps need to use a linear format (UNORM) to preserve values exactly
		vulkanFormat = VK_FORMAT_R8G8B8A8_UNORM;

		/// Ensure normal maps have 4 channels (RGBA)
		if (textureData.channels == 3) {
			/// Convert RGB to RGBA by adding alpha channel
			spdlog::debug("Converting RGB to RGBA for embedded normal map");
			std::vector<uint8_t> rgbaData(textureData.width * textureData.height * 4);
			for (int i = 0; i < textureData.width * textureData.height; i++) {
				rgbaData[i * 4 + 0] = textureData.pixels[i * 3 + 0]; /// R
				rgbaData[i * 4 + 1] = textureData.pixels[i * 3 + 1]; /// G
				rgbaData[i * 4 + 2] = textureData.pixels[i * 3 + 2]; /// B
				rgbaData[i * 4 + 3] = 255;                           /// A (fully opaque)
			}
			/// Swap the pixel data
			textureData.pixels = std::move(rgbaData);
			textureData.channels = 4;
		}
	} else {
		/// For standard textures, select format based on channel count
		switch (textureData.channels) {
		case 1:
			vulkanFormat = VK_FORMAT_R8_UNORM;
			break;
		case 2:
			vulkanFormat = VK_FORMAT_R8G8_UNORM;
			break;
		case 3: {
			/// Convert RGB to RGBA for better compatibility with Vulkan
			spdlog::debug("Converting RGB to RGBA for embedded texture");
			std::vector<uint8_t> rgbaData(textureData.width * textureData.height * 4);
			for (int i = 0; i < textureData.width * textureData.height; i++) {
				rgbaData[i * 4 + 0] = textureData.pixels[i * 3 + 0]; /// R
				rgbaData[i * 4 + 1] = textureData.pixels[i * 3 + 1]; /// G
				rgbaData[i * 4 + 2] = textureData.pixels[i * 3 + 2]; /// B
				rgbaData[i * 4 + 3] = 255;                           /// A (fully opaque)
			}
			/// Swap the pixel data
			textureData.pixels = std::move(rgbaData);
			textureData.channels = 4;
			vulkanFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		}
		case 4:
			vulkanFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		default:
			spdlog::warn("Unsupported channel count {} for embedded texture, falling back to RGBA",
				textureData.channels);
			vulkanFormat = VK_FORMAT_R8G8B8A8_SRGB;
		}
	}

	/// Calculate appropriate mip levels
	/// We either use automatic calculation (when generateMipmaps=true) or a single level
	uint32_t mipLevels = generateMipmaps ? 0 : 1; /// 0 means calculate automatically

	try {
		/// Create the GPU texture resource for the embedded texture
		auto texture = std::make_shared<Texture>(
			this->device,
			this->physicalDevice,
			static_cast<uint32_t>(textureData.width),
			static_cast<uint32_t>(textureData.height),
			vulkanFormat,
			mipLevels,
			1, /// Single layer
			name /// Use the provided name for identification
		);

		/// Upload the pixel data to the GPU
		/// This transfers the decoded image data to the GPU texture
		texture->uploadData(
			textureData.pixels.data(),
			textureData.pixels.size(),
			this->commandPool,
			this->graphicsQueue,
			*this->commandBufferManager.get()
		);

		/// Configure default sampler settings
		/// These settings work well for most embedded textures
		texture->configureSampler(
			Texture::FilterMode::Linear,
			Texture::FilterMode::Linear,
			Texture::WrapMode::Repeat,
			Texture::WrapMode::Repeat,
			true, /// Enable anisotropic filtering
			16.0f /// Max anisotropy
		);

		/// Cache the texture for future use
		/// This is especially important for models that might reuse the same texture
		{
			std::lock_guard<std::mutex> lock(this->cacheMutex);
			this->textureCache[name] = texture;
		}

		spdlog::info("Created and cached embedded texture: {}, {}x{}, {} channels",
			name, textureData.width, textureData.height, textureData.channels);
		return texture;
	}
	catch (const vulkan::VulkanException& e) {
		/// Handle Vulkan errors during texture creation
		spdlog::error("Failed to create embedded texture '{}': {}", name, e.what());
		return this->getDefaultTexture();
	}
	catch (const std::exception& e) {
		/// Handle any other unexpected errors
		spdlog::error("Unexpected error creating embedded texture '{}': {}", name, e.what());
		return this->getDefaultTexture();
	}
}

bool TextureManager::isTextureLoaded(const std::string& filename) const {
	/// Normalize the path for consistent lookup
	std::string normalizedPath = this->normalizePath(filename);

	/// Check if the texture exists in the cache
	std::lock_guard<std::mutex> lock(this->cacheMutex);
	return this->textureCache.find(normalizedPath) != this->textureCache.end();
}

std::shared_ptr<Texture> TextureManager::getTexture(const std::string& name) const {
	/// Look up the texture in the cache
	std::lock_guard<std::mutex> lock(this->cacheMutex);

	auto it = this->textureCache.find(name);
	if (it != this->textureCache.end()) {
		return it->second;
	}

	/// Return nullptr if not found, allowing caller to handle missing textures
	spdlog::debug("Texture '{}' not found in cache", name);
	return nullptr;
}

bool TextureManager::releaseTexture(const std::string& name) {
	std::lock_guard<std::mutex> lock(this->cacheMutex);

	auto it = this->textureCache.find(name);
	if (it != this->textureCache.end()) {
		spdlog::debug("Releasing texture '{}' from cache (use count before: {})",
			name, it->second.use_count());
		/// Erase will decrement the shared_ptr reference count
		/// The texture will be destroyed if no other references exist
		this->textureCache.erase(it);
		return true;
	}

	return false;
}

void TextureManager::releaseAllTextures() {
	std::lock_guard<std::mutex> lock(this->cacheMutex);

	size_t count = this->textureCache.size();
	if (count > 0) {
		spdlog::info("Releasing all {} textures from cache", count);

		for (const auto& [name, texture] : this->textureCache) {
			spdlog::debug("Releasing cached texture '{}' (use count: {})",
				name, texture.use_count());
		}

		this->textureCache.clear();
	}

	/// Also release the default texture if it exists
	if (this->defaultTexture) {
		spdlog::debug("Releasing default texture (use count: {})",
			this->defaultTexture.use_count());
		this->defaultTexture.reset();
	}
}

std::shared_ptr<Texture> TextureManager::createDefaultTexture() {
	/// Create a small white texture as a default fallback
	/// A single white pixel works well as a default for most material systems
	const uint32_t size = 4; /// Using 4x4 instead of 1x1 to support mipmaps
	const uint32_t channels = 4; /// RGBA
	std::vector<uint8_t> whitePixels(size * size * channels, 255); /// All white, opaque

	try {
		/// Create the default texture with a recognizable name
		auto texture = std::make_shared<Texture>(
			this->device,
			this->physicalDevice,
			size,
			size,
			VK_FORMAT_R8G8B8A8_SRGB,
			0, // Generate mipmaps automatically
			1, // Single layer
			"__default"
		);

		/// Upload the white pixel data
		texture->uploadData(
			whitePixels.data(),
			whitePixels.size(),
			this->commandPool,
			this->graphicsQueue,
			*this->commandBufferManager.get()
		);

		/// Configure sampler to repeat for seamless tiling
		texture->configureSampler(
			Texture::FilterMode::Linear,
			Texture::FilterMode::Linear,
			Texture::WrapMode::Repeat,
			Texture::WrapMode::Repeat,
			false, // No need for anisotropic filtering on a solid color
			1.0f
		);

		spdlog::info("Created default white texture");
		return texture;
	}
	catch (const vulkan::VulkanException& e) {
		/// Handle failure to create default texture
		/// This is a critical error but we return nullptr instead of throwing
		/// to avoid crash loops if default texture creation repeatedly fails
		spdlog::critical("Failed to create default texture: {}", e.what());
		return nullptr;
	}
}

std::shared_ptr<Texture> TextureManager::getDefaultTexture() {
	/// Create the default texture on demand if it doesn't exist
	if (!this->defaultTexture) {
		this->defaultTexture = this->createDefaultTexture();

		/// Cache the default texture for future use
		if (this->defaultTexture) {
			std::lock_guard<std::mutex> lock(this->cacheMutex);
			this->textureCache["__default"] = this->defaultTexture;
		}
	}

	return this->defaultTexture;
}

std::string TextureManager::normalizePath(const std::string& path) const {
	/// Normalize paths to ensure consistent lookup regardless of format
	/// This converts paths like "textures/../textures/grass.png" to "textures/grass.png"
	try {
		std::filesystem::path fsPath = std::filesystem::absolute(path).lexically_normal();
		return fsPath.string();
	}
	catch (const std::exception& e) {
		/// If path normalization fails, return the original path
		/// This is a fallback to avoid breaking texture loading
		spdlog::warn("Failed to normalize path '{}': {}", path, e.what());
		return path;
	}
}

} /// namespace lillugsi::rendering