#pragma once

#include "modeldata.h"
#include <memory>
#include <string>
#include <filesystem>

namespace tinygltf {
	class Model;
	class Material;
}

namespace lillugsi::rendering {

/// MaterialExtractor handles conversion of glTF materials to our engine's format
/// We use a dedicated class to encapsulate the complexity of material extraction
/// and keep the model loader focused on higher-level concerns
class MaterialExtractor {
public:
	/// Create a material extractor
	/// @param gltfModel The glTF model containing material data
	explicit MaterialExtractor(const tinygltf::Model& gltfModel);
	
	/// Extract material information from a glTF material
	/// @param materialIndex Index of the material in the glTF model
	/// @param baseDir Base directory for resolving texture paths
	/// @return The extracted material info ready for creating engine materials
	[[nodiscard]] ModelData::MaterialInfo extractMaterialInfo(
		int materialIndex,
		const std::string& baseDir) const;
		
	/// Get the name for a material
	/// @param materialIndex Index of the material in the glTF model
	/// @return A unique name for the material
	[[nodiscard]] std::string getMaterialName(int materialIndex) const;
	
	/// Extract all materials from the glTF model
	/// @param baseDir Base directory for resolving texture paths
	/// @return Map of material names to their extracted information
	[[nodiscard]] std::unordered_map<std::string, ModelData::MaterialInfo> extractAllMaterials(
		const std::string& baseDir) const;
		
private:
	/// Get texture path from glTF texture reference
	/// @param textureIndex Index of the texture in the glTF model
	/// @param baseDir Base directory for resolving paths
	/// @return Full path to the texture file
	[[nodiscard]] std::string getTexturePath(
		int textureIndex,
		const std::string& baseDir) const;
		
	/// Extract emissive properties from glTF material
	/// @param material The glTF material to extract from
	/// @param materialInfo The material info to fill
	/// @param baseDir Base directory for resolving texture paths
	void extractEmissiveProperties(
		const tinygltf::Material& material,
		ModelData::MaterialInfo& materialInfo,
		const std::string& baseDir) const;
		
	/// Extract transparency properties from glTF material
	/// @param material The glTF material to extract from
	/// @param materialInfo The material info to fill
	void extractTransparencyProperties(
		const tinygltf::Material& material,
		ModelData::MaterialInfo& materialInfo) const;
		
	/// The glTF model being processed
	const tinygltf::Model& gltfModel;
};

} /// namespace lillugsi::rendering