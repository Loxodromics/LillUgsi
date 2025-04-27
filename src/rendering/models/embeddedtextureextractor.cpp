#include "embeddedtextureextractor.h"
#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace lillugsi::rendering::models {

EmbeddedTextureExtractor::EmbeddedTextureExtractor(std::shared_ptr<TextureManager> textureManager)
	: textureManager(std::move(textureManager)) {
	spdlog::info("Embedded texture extractor created");
}

size_t EmbeddedTextureExtractor::extractTextures(
	const tinygltf::Model& gltfModel,
	const std::string& modelName,
	bool generateMipmaps) {

	/// Clear previous texture mappings
	/// This ensures we start with a clean state for each model
	this->textureMap.clear();

	/// Track successful extractions for return value and logging
	size_t extractedCount = 0;

	/// Log the starting extraction process
	spdlog::info("Extracting embedded textures from model '{}'", modelName);

	/// First, scan all textures to build a mapping from texture to image indices
	/// This is important because glTF separates textures and images - a texture
	/// references an image plus sampler settings
	std::unordered_map<int, int> textureToImageMap;
	for (size_t i = 0; i < gltfModel.textures.size(); ++i) {
		const auto& texture = gltfModel.textures[i];
		if (texture.source >= 0 && texture.source < static_cast<int>(gltfModel.images.size())) {
			textureToImageMap[static_cast<int>(i)] = texture.source;
			spdlog::debug("Mapped texture {} to image {}", i, texture.source);
		}
	}

	/// Process each image in the model
	for (size_t i = 0; i < gltfModel.images.size(); ++i) {
		const auto& image = gltfModel.images[i];
		
		/// We're only interested in images stored in buffer views
		/// Images with URIs are handled elsewhere in the texture loading system
		if (image.uri.empty() && image.bufferView >= 0) {
			/// Generate a unique name for this embedded texture
			std::string textureName = this->generateTextureName(
				modelName, 
				static_cast<int>(i), 
				image.name
			);
			
			/// Extract the image from the buffer view
			bool success = this->extractImage(
				gltfModel,
				static_cast<int>(i),
				textureName,
				generateMipmaps
			);
			
			if (success) {
				/// For each texture that uses this image, add it to our texture map
				for (const auto& [textureIndex, imageIndex] : textureToImageMap) {
					if (imageIndex == static_cast<int>(i)) {
						this->textureMap[textureIndex] = textureName;
						spdlog::debug("Registered texture {} with name '{}'", textureIndex, textureName);
					}
				}
				extractedCount++;
			}
		}
		else if (!image.uri.empty()) {
			/// For images with URIs, we don't extract them directly
			/// Instead, we just keep track of which textures use external images
			/// This ensures our material system knows to look for these textures
			for (const auto& [textureIndex, imageIndex] : textureToImageMap) {
				if (imageIndex == static_cast<int>(i)) {
					spdlog::debug("Texture {} uses external image URI: {}", textureIndex, image.uri);
					this->textureMap[textureIndex] = image.uri;
				}
			}
		}
	}

	spdlog::info("Extracted {} embedded textures from model '{}'", extractedCount, modelName);
	return extractedCount;
}

bool EmbeddedTextureExtractor::extractImage(
	const tinygltf::Model& gltfModel,
	int imageIndex,
	const std::string& textureName,
	bool generateMipmaps) {

	/// Validate image index
	if (imageIndex < 0 || imageIndex >= static_cast<int>(gltfModel.images.size())) {
		spdlog::error("Invalid image index: {}", imageIndex);
		return false;
	}

	const auto& image = gltfModel.images[imageIndex];

	/// Validate buffer view index
	if (image.bufferView < 0 || image.bufferView >= static_cast<int>(gltfModel.bufferViews.size())) {
		spdlog::error("Invalid buffer view index: {}", image.bufferView);
		return false;
	}

	const auto& bufferView = gltfModel.bufferViews[image.bufferView];

	/// Validate buffer index
	if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(gltfModel.buffers.size())) {
		spdlog::error("Invalid buffer index: {}", bufferView.buffer);
		return false;
	}

	const auto& buffer = gltfModel.buffers[bufferView.buffer];

	/// Calculate buffer offset and size
	/// The buffer view defines where in the buffer our image data starts
	/// and how many bytes it occupies
	const size_t offset = bufferView.byteOffset;
	const size_t size = bufferView.byteLength;

	/// Validate buffer size to prevent out-of-bounds access
	if (offset + size > buffer.data.size()) {
		spdlog::error("Buffer view exceeds buffer size for image {}", imageIndex);
		return false;
	}

	/// Get pointer to the image data in the buffer
	const void* bufferData = buffer.data.data() + offset;

	/// Determine the appropriate format based on the image's MIME type
	/// Different image formats require different loading parameters
	auto format = this->determineTextureFormat(image.mimeType);

	/// Use the TextureManager to create a texture from this buffer view
	/// This handles the decoding and GPU upload of the image data
	auto texture = this->textureManager->createTextureFromBufferView(
		textureName,
		bufferData,
		size,
		image.mimeType,
		generateMipmaps,
		format
	);

	/// Check if texture creation was successful
	/// The TextureManager will return a default texture on failure,
	/// so we need to make sure we got a valid texture
	if (!texture) {
		spdlog::error("Failed to create texture from buffer view for image {}", imageIndex);
		return false;
	}

	spdlog::debug("Extracted embedded texture '{}' ({}x{}) from image {}",
		textureName, texture->getWidth(), texture->getHeight(), imageIndex);
	return true;
}

std::string EmbeddedTextureExtractor::getTextureName(int textureIndex) const {
	auto it = this->textureMap.find(textureIndex);
	if (it != this->textureMap.end()) {
		return it->second;
	}
	
	/// Return empty string if texture not found
	/// This allows the caller to distinguish between existing and non-existing textures
	return "";
}

bool EmbeddedTextureExtractor::hasTexture(int textureIndex) const {
	return this->textureMap.find(textureIndex) != this->textureMap.end();
}

std::string EmbeddedTextureExtractor::generateTextureName(
	const std::string& modelName,
	int imageIndex,
	const std::string& imageName) const {
	
	/// Create a unique texture name that includes:
	/// 1. The model name as a prefix for namespace isolation
	/// 2. The image index to ensure uniqueness within the model
	/// 3. The image name if available for better debugging
	
	std::string baseName = modelName;
	
	/// Clean up model name to create a valid identifier
	/// Remove any file extension and problematic characters
	std::filesystem::path modelPath(modelName);
	baseName = modelPath.stem().string();
	
	/// Create a name pattern:
	/// embedded_[modelName]_[index]_[imageName]
	std::string textureName = "embedded_" + baseName + "_" + std::to_string(imageIndex);
	
	/// Add image name if available
	if (!imageName.empty()) {
		/// Replace any problematic characters in the image name
		std::string cleanImageName = imageName;
		std::replace(cleanImageName.begin(), cleanImageName.end(), '/', '_');
		std::replace(cleanImageName.begin(), cleanImageName.end(), '\\', '_');
		std::replace(cleanImageName.begin(), cleanImageName.end(), ' ', '_');
		
		textureName += "_" + cleanImageName;
	}
	
	return textureName;
}

TextureLoader::Format EmbeddedTextureExtractor::determineTextureFormat(
	const std::string& mimeType) const {
	
	/// Choose appropriate texture format based on MIME type
	/// This ensures we load the image data correctly for each format
	
	/// For normal maps (no direct way to detect, would need material info)
	/// Caller would need to handle this specifically if needed
	
	/// For most color textures, RGBA is appropriate
	/// We use RGBA for maximum compatibility and simplicity
	/// The TextureManager will handle channel conversion if needed
	return TextureLoader::Format::RGBA;
}

} /// namespace lillugsi::rendering::models