#pragma once

#include <vector>
#include <string>
#include <memory>

namespace lillugsi::rendering {

/// TextureLoader provides functionality for loading texture data from files
/// This class encapsulates image loading logic and provides a consistent interface
/// for working with various image formats through stb_image
class TextureLoader {
public:
	/// Contains the result of loading a texture
	/// This struct bundles all the data and metadata from an image load operation
	struct TextureData {
		std::vector<uint8_t> pixels;  /// Raw pixel data in requested format
		int width{0};                 /// Width of the image in pixels
		int height{0};                /// Height of the image in pixels
		int channels{0};              /// Number of color channels (e.g., 3 for RGB, 4 for RGBA)
		bool success{false};          /// Whether the load operation was successful
		std::string errorMessage;     /// Error message if loading failed
	};

	/// Image format options for texture loading
	/// These determine the desired output format after loading
	enum class Format {
		Keep,    /// Keep the format from the source file
		RGB,     /// Convert to RGB (3 channels)
		RGBA     /// Convert to RGBA (4 channels)
	};

	/// Load texture data from a file
	/// This method decodes the image file into raw pixel data
	///
	/// @param filename Path to the image file
	/// @param format Desired output format for the pixel data
	/// @param flipVertically Whether to flip the image vertically during loading
	/// @return TextureData containing the loaded pixel data and metadata
	[[nodiscard]] static TextureData loadFromFile(
		const std::string& filename,
		Format format = Format::RGBA,
		bool flipVertically = true
	);

	/// Load texture data from memory
	/// Useful for loading textures from embedded resources or network data
	///
	/// @param data Pointer to the encoded image data
	/// @param size Size of the data in bytes
	/// @param format Desired output format for the pixel data
	/// @param flipVertically Whether to flip the image vertically during loading
	/// @return TextureData containing the loaded pixel data and metadata
	[[nodiscard]] static TextureData loadFromMemory(
		const void* data,
		size_t size,
		Format format = Format::RGBA,
		bool flipVertically = true
	);

private:
	/// Convert our format enum to stb's desired channels parameter
	/// @param format The format to convert
	/// @return The number of channels for stb_image (0 = keep original)
	[[nodiscard]] static int formatToChannels(Format format);
};

} /// namespace lillugsi::rendering