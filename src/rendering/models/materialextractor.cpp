#include "materialextractor.h"
#include <tiny_gltf.h>
#include <spdlog/spdlog.h>

namespace lillugsi::rendering::models {

MaterialExtractor::MaterialExtractor(const tinygltf::Model& gltfModel)
	: gltfModel(gltfModel)
	, embeddedTextureExtractor(nullptr) {
}

void MaterialExtractor::setEmbeddedTextureExtractor(std::shared_ptr<EmbeddedTextureExtractor> extractor) {
	this->embeddedTextureExtractor = std::move(extractor);
	spdlog::debug("Embedded texture extractor set for material extraction");
}

ModelData::MaterialInfo MaterialExtractor::extractMaterialInfo(
	int materialIndex,
	const std::string& baseDir) const {

	ModelData::MaterialInfo materialInfo;

	/// Validate material index
	if (materialIndex < 0 || materialIndex >= static_cast<int>(this->gltfModel.materials.size())) {
		spdlog::error("Invalid material index: {}", materialIndex);
		return materialInfo;
	}

	const auto& material = this->gltfModel.materials[materialIndex];

	/// Extract base color factor
	/// glTF PBR materials use a baseColorFactor for the albedo color
	if (material.pbrMetallicRoughness.baseColorFactor.size() >= 3) {
		materialInfo.baseColor = glm::vec4(
			material.pbrMetallicRoughness.baseColorFactor[0],
			material.pbrMetallicRoughness.baseColorFactor[1],
			material.pbrMetallicRoughness.baseColorFactor[2],
			material.pbrMetallicRoughness.baseColorFactor.size() >= 4 ?
				material.pbrMetallicRoughness.baseColorFactor[3] : 1.0f
		);
	}

	/// Extract metallic factor
	/// glTF uses 0 for dielectric, 1 for metallic
	materialInfo.metallic = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);

	/// Extract roughness factor
	/// glTF uses 0 for smooth, 1 for rough
	materialInfo.roughness = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);

	/// Extract base color (albedo) texture
	/// This defines the main color of the material
	if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
		materialInfo.albedoTexturePath = this->getTexturePath(
			material.pbrMetallicRoughness.baseColorTexture.index, baseDir);

		/// Extract texture coordinate set if specified
		/// glTF supports multiple UV sets, but we currently only use the first set
		if (material.pbrMetallicRoughness.baseColorTexture.texCoord > 0) {
			spdlog::warn("Material '{}' uses texCoord set {} for base color, but we only support set 0",
				material.name, material.pbrMetallicRoughness.baseColorTexture.texCoord);
		}

		/// Log details about found texture
		spdlog::debug("Material '{}' uses albedo texture: {}",
			material.name, materialInfo.albedoTexturePath);
	}

	/// Extract normal map texture and scale
	/// Normal maps provide surface detail without added geometry
	if (material.normalTexture.index >= 0) {
		materialInfo.normalTexturePath = this->getTexturePath(
			material.normalTexture.index, baseDir);

		/// Extract normal scale factor
		/// This controls how strong the normal map effect is
		materialInfo.normalScale = static_cast<float>(material.normalTexture.scale);

		if (materialInfo.normalScale == 0.0f) {
			/// Default to 1.0 if not specified as 0 would make the normal map have no effect
			materialInfo.normalScale = 1.0f;
		}

		spdlog::debug("Material '{}' uses normal map: {} (scale: {})",
			material.name, materialInfo.normalTexturePath, materialInfo.normalScale);
	}

	/// Extract metallic-roughness texture
	/// glTF stores metallic in B channel, roughness in G channel of the same texture
	if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
		std::string texturePath = this->getTexturePath(
			material.pbrMetallicRoughness.metallicRoughnessTexture.index, baseDir);

		/// Store the same texture path for both roughness and metallic
		/// This is efficient as these are often packed together in glTF
		materialInfo.metallicTexturePath = texturePath;
		materialInfo.roughnessTexturePath = texturePath;

		spdlog::debug("Material '{}' uses combined metallic-roughness texture: {}",
			material.name, texturePath);
	}

	/// Extract occlusion texture and strength
	/// Occlusion maps darken areas that receive less ambient light
	if (material.occlusionTexture.index >= 0) {
		materialInfo.occlusionTexturePath = this->getTexturePath(
			material.occlusionTexture.index, baseDir);

		/// Extract occlusion strength
		/// This controls how much the occlusion map darkens the material
		materialInfo.occlusion = static_cast<float>(material.occlusionTexture.strength);

		if (materialInfo.occlusion == 0.0f) {
			/// Default to 1.0 if not specified as 0 would make the occlusion map have no effect
			materialInfo.occlusion = 1.0f;
		}

		spdlog::debug("Material '{}' uses occlusion map: {} (strength: {})",
			material.name, materialInfo.occlusionTexturePath, materialInfo.occlusion);
	}

	/// Extract emissive properties
	/// Emissive materials appear to emit light (though don't actually illuminate other objects)
	this->extractEmissiveProperties(material, materialInfo, baseDir);

	/// Extract transparency properties
	/// This handles both the alpha mode and alpha cutoff settings
	this->extractTransparencyProperties(material, materialInfo);

	/// Extract double-sided flag
	/// Double-sided materials render both front and back faces
	materialInfo.doubleSided = material.doubleSided;
	if (material.doubleSided) {
		spdlog::debug("Material '{}' is double-sided", material.name);
	}

	/// Extract custom extensions if present
	/// Many materials use extensions for additional properties beyond the core spec
	if (!material.extensions.empty()) {
		/// Log extensions for debugging
		for (const auto& extension : material.extensions) {
			spdlog::debug("Material '{}' uses extension: {}", material.name, extension.first);
		}

		/// Handle KHR_materials_unlit extension
		/// This indicates a material that doesn't use PBR lighting
		if (material.extensions.find("KHR_materials_unlit") != material.extensions.end()) {
			materialInfo.unlit = true;
			spdlog::debug("Material '{}' is unlit", material.name);
		}

		/// Other extensions could be processed here as needed
		/// For example: KHR_materials_clearcoat, KHR_materials_specular, etc.
	}

	return materialInfo;
}

std::string MaterialExtractor::getMaterialName(int materialIndex) const {
	/// Validate material index
	if (materialIndex < 0 || materialIndex >= static_cast<int>(this->gltfModel.materials.size())) {
		return "material_" + std::to_string(materialIndex);
	}

	const auto& material = this->gltfModel.materials[materialIndex];

	/// Use the material's name if available
	/// Otherwise generate a name based on the index
	if (!material.name.empty()) {
		return material.name;
	} else {
		return "material_" + std::to_string(materialIndex);
	}
}

std::unordered_map<std::string, ModelData::MaterialInfo> MaterialExtractor::extractAllMaterials(
	const std::string& baseDir) const {

	std::unordered_map<std::string, ModelData::MaterialInfo> materials;

	/// Process all materials in the glTF model
	for (size_t i = 0; i < this->gltfModel.materials.size(); ++i) {
		std::string materialName = this->getMaterialName(static_cast<int>(i));
		ModelData::MaterialInfo materialInfo = this->extractMaterialInfo(static_cast<int>(i), baseDir);
		materials[materialName] = std::move(materialInfo);
	}

	/// If no materials were found, create a default material
	/// This ensures we always have at least one material to work with
	if (materials.empty() && !this->gltfModel.meshes.empty()) {
		spdlog::debug("No materials found in model, creating default material");

		ModelData::MaterialInfo defaultMaterial;
		defaultMaterial.baseColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
		defaultMaterial.roughness = 0.5f;
		defaultMaterial.metallic = 0.0f;

		materials["default_material"] = std::move(defaultMaterial);
	}

	spdlog::info("Extracted {} materials from glTF model", materials.size());
	return materials;
}

std::string MaterialExtractor::getTexturePath(
	int textureIndex,
	const std::string& baseDir) const {

	/// Validate texture index
	if (textureIndex < 0 || textureIndex >= static_cast<int>(this->gltfModel.textures.size())) {
		spdlog::error("Invalid texture index: {}", textureIndex);
		return "";
	}

	/// First check if this is an embedded texture
	/// If we have an embedded texture extractor and it knows about this texture,
	/// return the embedded texture identifier instead of trying to resolve a file path
	if (this->embeddedTextureExtractor && this->embeddedTextureExtractor->hasTexture(textureIndex)) {
		std::string embeddedTextureName = this->embeddedTextureExtractor->getTextureName(textureIndex);

		/// If we have a valid embedded texture name, use it
		if (!embeddedTextureName.empty()) {
			spdlog::debug("Using embedded texture '{}' for texture index {}",
				embeddedTextureName, textureIndex);
			return embeddedTextureName;
		}
	}

	/// If it's not an embedded texture, proceed with normal file path resolution
	const auto& texture = this->gltfModel.textures[textureIndex];

	/// Validate image index
	if (texture.source < 0 || texture.source >= static_cast<int>(this->gltfModel.images.size())) {
		spdlog::error("Invalid image index in texture: {}", texture.source);
		return "";
	}

	const auto& image = this->gltfModel.images[texture.source];

	/// Check if the image is embedded (data URI) or external (file path)
	if (!image.uri.empty()) {
		/// If the image is an external file, resolve the path
		/// We need to handle relative paths correctly

		/// Check if the URI is a data URI
		if (image.uri.find("data:") == 0) {
			/// Data URIs are embedded in the glTF file
			/// This would require extracting the data and saving to a temporary file
			/// For this implementation, we'll just log a warning
			spdlog::warn("Data URI textures not fully supported: {}", image.uri.substr(0, 30) + "...");
			return "";
		}

		/// Construct the full path by combining base directory and relative path
		std::filesystem::path imagePath = baseDir.empty() ?
			std::filesystem::path(image.uri) :
			std::filesystem::path(baseDir) / image.uri;

		/// Return the normalized path
		return imagePath.lexically_normal().string();
	} else if (image.bufferView >= 0) {
		/// This is an embedded texture but no embedded texture extractor was provided
		/// or it doesn't have information about this texture
		spdlog::warn("Found embedded buffer texture without proper extractor configuration");
		return "";
	}

	/// If we reach here, the image data couldn't be found
	spdlog::error("Texture data not found for texture index: {}", textureIndex);
	return "";
}

void MaterialExtractor::extractEmissiveProperties(
	const tinygltf::Material& material,
	ModelData::MaterialInfo& materialInfo,
	const std::string& baseDir) const {

	/// Extract emissive factor
	/// This defines the color and intensity of self-illumination
	if (material.emissiveFactor.size() >= 3) {
		materialInfo.emissiveColor = glm::vec3(
			material.emissiveFactor[0],
			material.emissiveFactor[1],
			material.emissiveFactor[2]
		);

		/// Check if the material has any emission
		/// A non-zero emissive factor means the material emits light
		bool hasEmission = glm::length(materialInfo.emissiveColor) > 0.0f;
		materialInfo.emissive = hasEmission;

		if (hasEmission) {
			spdlog::debug("Material '{}' has emissive color: ({}, {}, {})",
				material.name,
				materialInfo.emissiveColor.r,
				materialInfo.emissiveColor.g,
				materialInfo.emissiveColor.b);
		}
	}

	/// Extract emissive texture if present
	/// This defines which parts of the surface emit light
	if (material.emissiveTexture.index >= 0) {
		materialInfo.emissiveTexturePath = this->getTexturePath(
			material.emissiveTexture.index, baseDir);

		/// Having an emissive texture means the material is emissive
		/// even if the emissive factor is zero
		materialInfo.emissive = true;

		spdlog::debug("Material '{}' uses emissive texture: {}",
			material.name, materialInfo.emissiveTexturePath);
	}
}

void MaterialExtractor::extractTransparencyProperties(
	const tinygltf::Material& material,
	ModelData::MaterialInfo& materialInfo) const {

	/// Extract alpha mode
	/// glTF defines three alpha modes: OPAQUE, MASK, and BLEND
	if (material.alphaMode == "OPAQUE") {
		materialInfo.alphaMode = ModelData::MaterialInfo::AlphaMode::Opaque;
	} else if (material.alphaMode == "MASK") {
		materialInfo.alphaMode = ModelData::MaterialInfo::AlphaMode::Mask;
		/// Extract alpha cutoff value for masked mode
		materialInfo.alphaCutoff = static_cast<float>(material.alphaCutoff);
		spdlog::debug("Material '{}' uses alpha masking with cutoff: {}",
			material.name, materialInfo.alphaCutoff);
	} else if (material.alphaMode == "BLEND") {
		materialInfo.alphaMode = ModelData::MaterialInfo::AlphaMode::Blend;
		spdlog::debug("Material '{}' uses alpha blending", material.name);
	} else {
		/// Default to opaque if not specified or unknown
		materialInfo.alphaMode = ModelData::MaterialInfo::AlphaMode::Opaque;
	}

	/// Set transparent flag based on alpha mode
	/// This makes it easier to check if a material uses transparency
	materialInfo.transparent = (materialInfo.alphaMode != ModelData::MaterialInfo::AlphaMode::Opaque);

	/// For alpha blending, also check base color alpha
	if (materialInfo.alphaMode == ModelData::MaterialInfo::AlphaMode::Blend &&
		materialInfo.baseColor.a < 1.0f) {
		spdlog::debug("Material '{}' has base color alpha: {}",
			material.name, materialInfo.baseColor.a);
	}
}

} /// namespace lillugsi::rendering::models