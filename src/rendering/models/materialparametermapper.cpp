#include "materialparametermapper.h"
#include <spdlog/spdlog.h>
#include <filesystem>

namespace lillugsi::rendering {

MaterialParameterMapper::MaterialParameterMapper(std::shared_ptr<TextureManager> textureManager)
	: textureManager(std::move(textureManager)) {
	
	spdlog::debug("Material parameter mapper created");
}

bool MaterialParameterMapper::applyParameters(
	std::shared_ptr<PBRMaterial> material,
	const ModelData::MaterialInfo& materialInfo,
	const std::string& basePath) {
	
	/// Check for valid material
	if (!material) {
		spdlog::error("Cannot apply parameters to null material");
		return false;
	}
	
	/// Track overall success status
	bool success = true;
	
	/// Apply basic scalar parameters first
	/// These are always applied regardless of texture availability
	if (!this->applyScalarParameters(material, materialInfo)) {
		spdlog::warn("Some scalar parameters failed to apply to material '{}'", 
			material->getName());
		success = false;
	}
	
	/// Apply textures if any are specified
	/// We treat texture application as optional - materials work without textures
	if (!this->applyTextures(material, materialInfo, basePath)) {
		spdlog::warn("Some textures failed to apply to material '{}'", 
			material->getName());
		success = false;
	}
	
	spdlog::info("Applied parameters to material '{}' (success: {})", 
		material->getName(), success ? "full" : "partial");
	
	/// Return whether all parameters were successfully applied
	/// Even partial success is useful - the material will still render
	return success;
}

bool MaterialParameterMapper::applyScalarParameters(
	std::shared_ptr<PBRMaterial> material,
	const ModelData::MaterialInfo& materialInfo) {
	
	/// Apply base color with alpha
	/// This sets the diffuse/albedo color and transparency
	material->setBaseColor(materialInfo.baseColor);
	
	/// Apply PBR metallic parameter
	/// Controls how metallic vs. dielectric the surface appears
	material->setMetallic(materialInfo.metallic);
	
	/// Apply PBR roughness parameter
	/// Controls microfacet distribution - how rough/smooth the surface appears
	material->setRoughness(materialInfo.roughness);
	
	/// Apply ambient occlusion factor
	/// Controls how much ambient light is occluded in crevices
	material->setAmbient(materialInfo.occlusion);
	
	/// Apply normal map strength if present
	/// Only relevant if a normal map texture is also applied
	if (!materialInfo.normalTexturePath.empty()) {
		material->setNormalStrength(materialInfo.normalScale);
	}
	
	spdlog::debug("Applied scalar parameters to material '{}': baseColor=({},{},{},{}), metallic={}, roughness={}, occlusion={}", 
		material->getName(),
		materialInfo.baseColor.r, 
		materialInfo.baseColor.g, 
		materialInfo.baseColor.b, 
		materialInfo.baseColor.a,
		materialInfo.metallic,
		materialInfo.roughness,
		materialInfo.occlusion);
	
	return true;
}

bool MaterialParameterMapper::applyTextures(
	std::shared_ptr<PBRMaterial> material,
	const ModelData::MaterialInfo& materialInfo,
	const std::string& basePath) {
	
	bool success = true;
	
	/// Track which textures have been applied
	/// This helps us detect and handle packed textures
	bool appliedRoughnessMetallic = false;
	bool appliedORM = false;
	
	/// Check for combined roughness-metallic texture
	/// Many models pack these two properties into different channels of one texture
	if (!materialInfo.roughnessTexturePath.empty() && 
		!materialInfo.metallicTexturePath.empty() &&
		this->isSameTexture(materialInfo.roughnessTexturePath, materialInfo.metallicTexturePath)) {
		
		/// Get the resolved texture path
		std::string texturePath = this->resolveTexturePath(
			materialInfo.roughnessTexturePath, basePath);
		
		/// Load the texture
		auto texture = this->textureManager->getOrLoadTexture(
			texturePath, true, TextureLoader::Format::RGBA);
		
		if (texture) {
			/// Apply as a combined roughness-metallic map
			/// In standard glTF PBR, roughness is in G channel, metallic in B
			material->setRoughnessMetallicMap(
				texture,
				Material::TextureChannel::G,  /// Roughness channel
				Material::TextureChannel::B,  /// Metallic channel
				materialInfo.roughness,       /// Roughness factor
				materialInfo.metallic         /// Metallic factor
			);
			
			spdlog::debug("Applied combined roughness-metallic texture to material '{}': {}", 
				material->getName(), texturePath);
			
			appliedRoughnessMetallic = true;
		} else {
			spdlog::warn("Failed to load combined roughness-metallic texture: {}", texturePath);
			success = false;
		}
	}
	
	/// Check for combined ORM (Occlusion-Roughness-Metallic) texture
	/// This is another common packing format in PBR workflows
	if (!appliedRoughnessMetallic &&
		!materialInfo.occlusionTexturePath.empty() &&
		!materialInfo.roughnessTexturePath.empty() &&
		!materialInfo.metallicTexturePath.empty() &&
		this->isSameTexture(materialInfo.occlusionTexturePath, materialInfo.roughnessTexturePath) &&
		this->isSameTexture(materialInfo.occlusionTexturePath, materialInfo.metallicTexturePath)) {
		
		/// Get the resolved texture path
		std::string texturePath = this->resolveTexturePath(
			materialInfo.occlusionTexturePath, basePath);
		
		/// Load the texture
		auto texture = this->textureManager->getOrLoadTexture(
			texturePath, true, TextureLoader::Format::RGBA);
		
		if (texture) {
			/// Apply as a combined ORM map with standard channel mapping
			/// Standard glTF: R=occlusion, G=roughness, B=metallic
			material->setOcclusionRoughnessMetallicMap(
				texture,
				Material::TextureChannel::R,  /// Occlusion channel
				Material::TextureChannel::G,  /// Roughness channel
				Material::TextureChannel::B,  /// Metallic channel
				materialInfo.occlusion,       /// Occlusion strength
				materialInfo.roughness,       /// Roughness factor
				materialInfo.metallic         /// Metallic factor
			);
			
			spdlog::debug("Applied combined ORM texture to material '{}': {}", 
				material->getName(), texturePath);
			
			appliedORM = true;
		} else {
			spdlog::warn("Failed to load combined ORM texture: {}", texturePath);
			success = false;
		}
	}
	
	/// Apply individual textures for properties not handled by combined maps
	
	/// Apply albedo (base color) texture if specified
	if (!materialInfo.albedoTexturePath.empty()) {
		if (!this->applyAlbedoTexture(
			material, 
			materialInfo.albedoTexturePath, 
			materialInfo.transparent, 
			basePath)) {
			success = false;
		}
	}
	
	/// Apply normal map if specified
	if (!materialInfo.normalTexturePath.empty()) {
		if (!this->applyNormalTexture(
			material, 
			materialInfo.normalTexturePath, 
			materialInfo.normalScale, 
			basePath)) {
			success = false;
		}
	}
	
	/// Apply individual property textures if they weren't handled by combined maps
	
	/// Apply roughness texture if not already handled
	if (!appliedRoughnessMetallic && !appliedORM && !materialInfo.roughnessTexturePath.empty()) {
		if (!this->applyRoughnessTexture(
			material, 
			materialInfo.roughnessTexturePath, 
			materialInfo.roughness, 
			basePath)) {
			success = false;
		}
	}
	
	/// Apply metallic texture if not already handled
	if (!appliedRoughnessMetallic && !appliedORM && !materialInfo.metallicTexturePath.empty()) {
		if (!this->applyMetallicTexture(
			material, 
			materialInfo.metallicTexturePath, 
			materialInfo.metallic, 
			basePath)) {
			success = false;
		}
	}
	
	/// Apply occlusion texture if not already handled
	if (!appliedORM && !materialInfo.occlusionTexturePath.empty()) {
		if (!this->applyOcclusionTexture(
			material, 
			materialInfo.occlusionTexturePath, 
			materialInfo.occlusion, 
			basePath)) {
			success = false;
		}
	}
	
	return success;
}

bool MaterialParameterMapper::applyAlbedoTexture(
	std::shared_ptr<PBRMaterial> material,
	const std::string& texturePath,
	bool isTransparent,
	const std::string& basePath) {
	
	/// Get the resolved texture path
	std::string resolvedPath = this->resolveTexturePath(texturePath, basePath);
	
	/// Choose the appropriate format based on transparency needs
	auto format = isTransparent ? 
		TextureLoader::Format::RGBA : /// Need alpha for transparency
		TextureLoader::Format::RGB;   /// Can save memory without alpha
	
	/// Load the texture through the texture manager
	auto texture = this->textureManager->getOrLoadTexture(resolvedPath, true, format);
	
	if (texture) {
		/// Apply the texture to the material
		material->setAlbedoTexture(texture);
		
		spdlog::debug("Applied albedo texture to material '{}': {}", 
			material->getName(), resolvedPath);
		return true;
	} else {
		spdlog::warn("Failed to load albedo texture for material '{}': {}", 
			material->getName(), resolvedPath);
		return false;
	}
}

bool MaterialParameterMapper::applyNormalTexture(
	std::shared_ptr<PBRMaterial> material,
	const std::string& texturePath,
	float strength,
	const std::string& basePath) {
	
	/// Get the resolved texture path
	std::string resolvedPath = this->resolveTexturePath(texturePath, basePath);
	
	/// Load the texture with special format for normal maps
	/// Normal maps need to be loaded in linear color space
	auto texture = this->textureManager->getOrLoadTexture(
		resolvedPath, true, TextureLoader::Format::NormalMap);
	
	if (texture) {
		/// Apply the normal map with specified strength
		material->setNormalMap(texture, strength);
		
		spdlog::debug("Applied normal map to material '{}': {} (strength: {})", 
			material->getName(), resolvedPath, strength);
		return true;
	} else {
		spdlog::warn("Failed to load normal map for material '{}': {}", 
			material->getName(), resolvedPath);
		return false;
	}
}

bool MaterialParameterMapper::applyRoughnessTexture(
	std::shared_ptr<PBRMaterial> material,
	const std::string& texturePath,
	float factor,
	const std::string& basePath) {
	
	/// Get the resolved texture path
	std::string resolvedPath = this->resolveTexturePath(texturePath, basePath);
	
	/// Load the texture - roughness typically only needs R channel
	auto texture = this->textureManager->getOrLoadTexture(
		resolvedPath, true, TextureLoader::Format::R);
	
	if (texture) {
		/// Apply the roughness map with specified factor
		material->setRoughnessMap(texture, factor);
		
		spdlog::debug("Applied roughness texture to material '{}': {} (factor: {})", 
			material->getName(), resolvedPath, factor);
		return true;
	} else {
		spdlog::warn("Failed to load roughness texture for material '{}': {}", 
			material->getName(), resolvedPath);
		return false;
	}
}

bool MaterialParameterMapper::applyMetallicTexture(
	std::shared_ptr<PBRMaterial> material,
	const std::string& texturePath,
	float factor,
	const std::string& basePath) {
	
	/// Get the resolved texture path
	std::string resolvedPath = this->resolveTexturePath(texturePath, basePath);
	
	/// Load the texture - metallic typically only needs R channel
	auto texture = this->textureManager->getOrLoadTexture(
		resolvedPath, true, TextureLoader::Format::R);
	
	if (texture) {
		/// Apply the metallic map with specified factor
		material->setMetallicMap(texture, factor);
		
		spdlog::debug("Applied metallic texture to material '{}': {} (factor: {})", 
			material->getName(), resolvedPath, factor);
		return true;
	} else {
		spdlog::warn("Failed to load metallic texture for material '{}': {}", 
			material->getName(), resolvedPath);
		return false;
	}
}

bool MaterialParameterMapper::applyOcclusionTexture(
	std::shared_ptr<PBRMaterial> material,
	const std::string& texturePath,
	float strength,
	const std::string& basePath) {
	
	/// Get the resolved texture path
	std::string resolvedPath = this->resolveTexturePath(texturePath, basePath);
	
	/// Load the texture - occlusion typically only needs R channel
	auto texture = this->textureManager->getOrLoadTexture(
		resolvedPath, true, TextureLoader::Format::R);
	
	if (texture) {
		/// Apply the occlusion map with specified strength
		material->setOcclusionMap(texture, strength);
		
		spdlog::debug("Applied occlusion texture to material '{}': {} (strength: {})", 
			material->getName(), resolvedPath, strength);
		return true;
	} else {
		spdlog::warn("Failed to load occlusion texture for material '{}': {}", 
			material->getName(), resolvedPath);
		return false;
	}
}

bool MaterialParameterMapper::isSameTexture(const std::string& path1, const std::string& path2) const {
	/// Handle empty paths
	if (path1.empty() || path2.empty()) {
		return false;
	}
	
	/// Normalize and compare the paths
	/// We use filesystem operations to handle different path formats
	try {
		std::filesystem::path normalizedPath1 = std::filesystem::path(path1).lexically_normal();
		std::filesystem::path normalizedPath2 = std::filesystem::path(path2).lexically_normal();
		
		return normalizedPath1 == normalizedPath2;
	} catch (const std::exception& e) {
		/// Handle filesystem exceptions - fall back to direct string comparison
		spdlog::warn("Path comparison error: {}, falling back to direct comparison", e.what());
		return path1 == path2;
	}
}

std::string MaterialParameterMapper::resolveTexturePath(
	const std::string& texturePath, 
	const std::string& basePath) const {
	
	/// Handle empty paths
	if (texturePath.empty()) {
		return "";
	}
	
	/// If the path is already absolute, return it directly
	std::filesystem::path path(texturePath);
	if (path.is_absolute()) {
		return texturePath;
	}
	
	/// If we have a base path, combine them
	if (!basePath.empty()) {
		std::filesystem::path result = std::filesystem::path(basePath) / path;
		return result.lexically_normal().string();
	}
	
	/// No base path, just normalize the relative path
	return path.lexically_normal().string();
}

} /// namespace lillugsi::rendering