#pragma once

#include "modeldata.h"
#include "rendering/pbrmaterial.h"
#include "rendering/texturemanager.h"
#include <memory>
#include <string>

namespace lillugsi::rendering {

/// MaterialParameterMapper handles the conversion between model material data
/// and engine-specific material parameters
///
/// We separate this mapping logic from the model loader to:
/// 1. Keep the model loader focused on file format specifics
/// 2. Allow different mapping strategies for different material systems
/// 3. Provide a clean interface for extending with new material features
class MaterialParameterMapper {
public:
	/// Create a material parameter mapper
	/// @param textureManager The texture manager for loading material textures
	explicit MaterialParameterMapper(std::shared_ptr<TextureManager> textureManager);
	
	/// Apply material parameters from model data to a PBR material
	/// This is the main mapping function that handles all parameter types
	/// @param material The target material to configure
	/// @param materialInfo The source material data from the model
	/// @param basePath Base path for resolving texture paths
	/// @return True if all parameters were successfully applied
	[[nodiscard]] bool applyParameters(
		std::shared_ptr<PBRMaterial> material,
		const ModelData::MaterialInfo& materialInfo,
		const std::string& basePath = "");
	
private:
	/// Apply the basic scalar parameters to the material
	/// These are the core PBR parameters like base color, metallic, roughness
	/// @param material The target material to configure
	/// @param materialInfo The source material data from the model
	/// @return True if parameters were successfully applied
	[[nodiscard]] bool applyScalarParameters(
		std::shared_ptr<PBRMaterial> material,
		const ModelData::MaterialInfo& materialInfo);
	
	/// Load and apply textures to the material
	/// This handles all texture-related parameters including maps for:
	/// albedo, normal, roughness, metallic, occlusion, etc.
	/// @param material The target material to configure
	/// @param materialInfo The source material data from the model
	/// @param basePath Base path for resolving texture paths
	/// @return True if textures were successfully applied
	[[nodiscard]] bool applyTextures(
		std::shared_ptr<PBRMaterial> material,
		const ModelData::MaterialInfo& materialInfo,
		const std::string& basePath);
	
	/// Load and apply the base color (albedo) texture
	/// @param material The target material to configure
	/// @param texturePath Path to the texture
	/// @param isTransparent Whether the material uses transparency
	/// @param basePath Base path for resolving the texture path
	/// @return True if the texture was successfully applied
	[[nodiscard]] bool applyAlbedoTexture(
		std::shared_ptr<PBRMaterial> material,
		const std::string& texturePath,
		bool isTransparent,
		const std::string& basePath);
	
	/// Load and apply the normal map texture
	/// @param material The target material to configure
	/// @param texturePath Path to the texture
	/// @param strength Normal map strength factor
	/// @param basePath Base path for resolving the texture path
	/// @return True if the texture was successfully applied
	[[nodiscard]] bool applyNormalTexture(
		std::shared_ptr<PBRMaterial> material,
		const std::string& texturePath,
		float strength,
		const std::string& basePath);
	
	/// Load and apply the roughness texture
	/// @param material The target material to configure
	/// @param texturePath Path to the texture
	/// @param factor Base roughness factor
	/// @param basePath Base path for resolving the texture path
	/// @return True if the texture was successfully applied
	[[nodiscard]] bool applyRoughnessTexture(
		std::shared_ptr<PBRMaterial> material,
		const std::string& texturePath,
		float factor,
		const std::string& basePath);
	
	/// Load and apply the metallic texture
	/// @param material The target material to configure
	/// @param texturePath Path to the texture
	/// @param factor Base metallic factor
	/// @param basePath Base path for resolving the texture path
	/// @return True if the texture was successfully applied
	[[nodiscard]] bool applyMetallicTexture(
		std::shared_ptr<PBRMaterial> material,
		const std::string& texturePath,
		float factor,
		const std::string& basePath);
	
	/// Load and apply the occlusion texture
	/// @param material The target material to configure
	/// @param texturePath Path to the texture
	/// @param strength Occlusion strength factor
	/// @param basePath Base path for resolving the texture path
	/// @return True if the texture was successfully applied
	[[nodiscard]] bool applyOcclusionTexture(
		std::shared_ptr<PBRMaterial> material,
		const std::string& texturePath,
		float strength,
		const std::string& basePath);
	
	/// Check if two texture paths reference the same texture
	/// This helps detect packed texture maps where multiple properties
	/// are stored in different channels of the same texture
	/// @param path1 First texture path
	/// @param path2 Second texture path
	/// @return True if the paths reference the same texture
	[[nodiscard]] bool isSameTexture(const std::string& path1, const std::string& path2) const;
	
	/// Resolve a texture path against a base directory
	/// This handles both absolute and relative paths
	/// @param texturePath The texture path to resolve
	/// @param basePath The base path for relative resolution
	/// @return The resolved absolute path
	[[nodiscard]] std::string resolveTexturePath(
		const std::string& texturePath, 
		const std::string& basePath) const;
	
	/// Texture manager for loading material textures
	std::shared_ptr<TextureManager> textureManager;
};

} /// namespace lillugsi::rendering