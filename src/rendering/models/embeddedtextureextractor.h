#pragma once

#include "rendering/texturemanager.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace tinygltf {
	class Model;
	class Image;
}

namespace lillugsi::rendering::models {

/// EmbeddedTextureExtractor handles the extraction and registration of textures
/// embedded in glTF/GLB files. This avoids the need to write temporary files to
/// disk and provides efficient texture reuse across multiple models.
class EmbeddedTextureExtractor {
public:
	/// Create a texture extractor with the given texture manager
	/// @param textureManager The texture manager to register extracted textures with
	explicit EmbeddedTextureExtractor(std::shared_ptr<TextureManager> textureManager);

	/// Extract and register all textures from a glTF model
	/// This processes all images in the model and extracts any that are
	/// embedded in buffer views rather than referenced by URI
	///
	/// @param gltfModel The parsed glTF model containing embedded textures
	/// @param modelName Base name for generating unique texture identifiers
	/// @param generateMipmaps Whether to generate mipmaps for extracted textures
	/// @return Number of textures successfully extracted
	[[nodiscard]] size_t extractTextures(
		const tinygltf::Model& gltfModel,
		const std::string& modelName,
		bool generateMipmaps = true);

	/// Get the cached texture name for a given texture index
	/// This provides the mapping between glTF texture indices and
	/// our engine's unique texture identifiers
	///
	/// @param textureIndex The glTF texture index to look up
	/// @return The engine texture name, or empty string if not found
	[[nodiscard]] std::string getTextureName(int textureIndex) const;

	/// Check if a texture with the given index was extracted
	/// @param textureIndex The glTF texture index to check
	/// @return True if the texture was successfully extracted
	[[nodiscard]] bool hasTexture(int textureIndex) const;

private:
	/// Extract a single image from the glTF model
	/// This handles the actual extraction of image data from buffer views
	///
	/// @param gltfModel The parsed glTF model containing the image
	/// @param imageIndex The index of the image to extract
	/// @param textureName The name to register the texture under
	/// @param generateMipmaps Whether to generate mipmaps for the texture
	/// @return True if the image was successfully extracted and registered
	[[nodiscard]] bool extractImage(
		const tinygltf::Model& gltfModel,
		int imageIndex,
		const std::string& textureName,
		bool generateMipmaps);

	/// Generate a unique texture name for an embedded texture
	/// This ensures no conflicts between textures from different models
	///
	/// @param modelName Base name of the model (used as prefix)
	/// @param imageIndex Index of the image in the glTF model
	/// @param imageName Name of the image from the glTF model (if available)
	/// @return A unique name for the texture
	[[nodiscard]] std::string generateTextureName(
		const std::string& modelName,
		int imageIndex,
		const std::string& imageName) const;

	/// Determine the appropriate texture format based on MIME type
	/// Different embedded formats need different loading settings
	///
	/// @param mimeType The MIME type of the image (e.g., "image/png")
	/// @return The appropriate texture format for loading
	[[nodiscard]] TextureLoader::Format determineTextureFormat(
		const std::string& mimeType) const;

	/// Mapping from glTF texture indices to engine texture names
	/// This allows us to quickly look up the corresponding texture
	/// for a given glTF material reference
	std::unordered_map<int, std::string> textureMap;

	/// The texture manager to register extracted textures with
	std::shared_ptr<TextureManager> textureManager;
};

} /// namespace lillugsi::rendering::models