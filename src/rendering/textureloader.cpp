#include "textureloader.h"

#include <spdlog/spdlog.h>

/// We define STB_IMAGE_IMPLEMENTATION in only one .cpp file
/// This is required to include the actual implementation, not just the header
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace lillugsi::rendering {

TextureLoader::TextureData TextureLoader::loadFromFile(
	const std::string& filename,
	Format format,
	bool flipVertically) {

	TextureData result;

	/// Set vertical flipping before loading
	/// This is important since OpenGL/Vulkan texture coordinates start at bottom-left
	/// while most image formats start at top-left
	stbi_set_flip_vertically_on_load(flipVertically);

	/// Convert our format enum to stb's desired channels
	int reqChannels = formatToChannels(format);

	/// Load the image
	/// stbi_load handles a variety of image formats (PNG, JPEG, etc.)
	int width, height, channels;
	unsigned char* data = stbi_load(
		filename.c_str(),
		&width, &height, &channels,
		reqChannels
	);

	if (!data) {
		/// If loading failed, populate error information
		/// Detailed error information helps with debugging
		result.success = false;
		result.errorMessage = "Failed to load texture: " + std::string(stbi_failure_reason());
		spdlog::error("Failed to load texture '{}': {}", filename, stbi_failure_reason());
		return result;
	}

	/// Store the actual number of channels in the loaded image
	/// If we requested a specific format, use that channel count instead
	int actualChannels = (reqChannels != 0) ? reqChannels : channels;

	/// Calculate total size of the pixel data
	/// This accounts for the image dimensions and channel count
	size_t dataSize = width * height * actualChannels;

	/// Copy the loaded data into our result vector
	/// We copy the data instead of using it directly because stbi_load
	/// allocates with malloc, and we want to use RAII with std::vector
	result.pixels.resize(dataSize);
	std::memcpy(result.pixels.data(), data, dataSize);

	/// Populate metadata in the result
	result.width = width;
	result.height = height;
	result.channels = actualChannels;
	result.success = true;

	/// Free the original stb-allocated memory
	/// This prevents memory leaks, as stb_image uses malloc/free
	stbi_image_free(data);

	spdlog::debug("Loaded texture '{}': {}x{}, {} channels, {} bytes",
		filename, width, height, actualChannels, dataSize);

	return result;
}

TextureLoader::TextureData TextureLoader::loadFromMemory(
	const void* data,
	size_t size,
	Format format,
	bool flipVertically) {

	TextureData result;

	if (!data || size == 0) {
		result.success = false;
		result.errorMessage = "Invalid input data (null pointer or zero size)";
		spdlog::error("Failed to load texture from memory: {}", result.errorMessage);
		return result;
	}

	/// Set vertical flipping before loading
	stbi_set_flip_vertically_on_load(flipVertically);

	/// Convert our format enum to stb's desired channels
	int reqChannels = formatToChannels(format);

	/// Load the image from memory
	/// stbi_load_from_memory works the same as stbi_load but uses in-memory data
	int width, height, channels;
	unsigned char* imgData = stbi_load_from_memory(
		static_cast<const stbi_uc*>(data),
		static_cast<int>(size),
		&width, &height, &channels,
		reqChannels
	);

	if (!imgData) {
		result.success = false;
		result.errorMessage = "Failed to load texture from memory: " + std::string(stbi_failure_reason());
		spdlog::error("Failed to load texture from memory: {}", stbi_failure_reason());
		return result;
	}

	/// Determine the actual channel count based on request
	int actualChannels = (reqChannels != 0) ? reqChannels : channels;

	/// Calculate and allocate memory for the result
	size_t dataSize = width * height * actualChannels;
	result.pixels.resize(dataSize);
	std::memcpy(result.pixels.data(), imgData, dataSize);

	/// Populate metadata
	result.width = width;
	result.height = height;
	result.channels = actualChannels;
	result.success = true;

	/// Free the original stb-allocated memory
	stbi_image_free(imgData);

	spdlog::debug("Loaded texture from memory: {}x{}, {} channels, {} bytes",
		width, height, actualChannels, dataSize);

	return result;
}

TextureLoader::TextureData TextureLoader::loadFromBufferView(
	const void* bufferData,
	size_t bufferSize,
	const std::string& mimeType,
	Format format,
	bool flipVertically) {

	TextureData result;

	/// Validate input parameters
	if (!bufferData || bufferSize == 0) {
		result.success = false;
		result.errorMessage = "Invalid buffer data (null pointer or zero size)";
		spdlog::error("Failed to load texture from buffer view: {}", result.errorMessage);
		return result;
	}

	/// Log detailed information about the buffer we're processing
	/// This helps with debugging embedded textures in glTF/GLB files
	spdlog::debug("Loading texture from buffer view: {} bytes, MIME type: {}",
		bufferSize, mimeType);

	/// We use the existing loadFromMemory method since the underlying operation
	/// is essentially the same: decoding image data from a memory buffer.
	/// This avoids duplicating the image loading logic while still providing
	/// a clear API for glTF buffer views.
	return loadFromMemory(bufferData, bufferSize, format, flipVertically);
}

int TextureLoader::formatToChannels(Format format) {
	/// Convert our format enum to the number of channels parameter used by stb_image
	/// stb_image uses 0 to mean "keep original format"
	switch (format) {
	case Format::Keep:
		return 0; /// Original format from the image
	case Format::RGB:
		return 3; /// Force RGB (3 channels)
	case Format::RGBA:
		return 4; /// Force RGBA (4 channels)
	case Format::R:
		return 1; /// Force R (1 channel)
	case Format::NormalMap:
		return 4; /// Force RGBA (4 channels)
	default:
		return 0; /// Fallback to original format
	}
}

} /// namespace lillugsi::rendering