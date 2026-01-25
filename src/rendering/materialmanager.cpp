#include "materialmanager.h"
#include "vulkan/vulkanexception.h"
#include <spdlog/spdlog.h>

#include <utility>

namespace lillugsi::rendering {

MaterialManager::MaterialManager(VkDevice device,
	VkPhysicalDevice physicalDevice,
	std::shared_ptr<TextureManager> textureManager)
	: device(device)
	, physicalDevice(physicalDevice)
	, textureManager(std::move(textureManager)) {
	spdlog::info("Material manager initialized");
}

MaterialManager::~MaterialManager() {
	this->cleanup();
}

std::shared_ptr<PBRMaterial> MaterialManager::createPBRMaterial(
	const std::string& name
) {
	/// Check if material already exists
	auto it = this->materials.find(name);
	if (it != this->materials.end()) {
		/// Try to cast existing material to PBRMaterial
		auto pbrMaterial = std::dynamic_pointer_cast<PBRMaterial>(it->second);
		if (pbrMaterial) {
			spdlog::debug("Returning existing PBR material '{}'", name);
			return pbrMaterial;
		}
		
		/// Material exists but is not a PBR material
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Material '" + name + "' exists but is not a PBR material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create new PBR material
	auto material = std::make_shared<PBRMaterial>(
		this->device,
		name,
		this->physicalDevice
	);
	
	/// Store in material map
	this->materials[name] = material;

	/// We do NOT set default textures for any material maps because that enables
	/// their "use" flags and overrides the uniform values. For normal maps specifically,
	/// the white default texture (1,1,1) becomes invalid tangent normal (1,1,1) which
	/// corrupts all lighting calculations.

	spdlog::info("Created new PBR material '{}'", name);
	return material;
}

std::shared_ptr<PBRMaterial> MaterialManager::createPBRMaterialWithCustomShaders(
	const std::string& name,
	const std::string& vertexShaderPath,
	const std::string& fragmentShaderPath
) {
	/// Validate material name before creation
	this->validateMaterialName(name);

	/// Create new PBR material with custom shaders
	auto material = std::make_shared<PBRMaterial>(
		this->device,
		name,
		this->physicalDevice,
		vertexShaderPath,
		fragmentShaderPath
	);

	/// Store in material map
	this->materials[name] = material;

	/// No default textures - see createPBRMaterial for explanation

	spdlog::info("Created new PBR material '{}' with custom shaders: {} and {}",
		name, vertexShaderPath, fragmentShaderPath);
	return material;
}

std::shared_ptr<CustomMaterial> MaterialManager::createCustomMaterial(
	const std::string& name,
	const std::string& vertexShaderPath,
	const std::string& fragmentShaderPath
) {
	/// Validate material name before creation
	this->validateMaterialName(name);

	/// Create new custom material
	auto material = std::make_shared<CustomMaterial>(
		this->device,
		name,
		this->physicalDevice,
		vertexShaderPath,
		fragmentShaderPath
	);
	
	/// Store in material map
	this->materials[name] = material;
	
	spdlog::info("Created new custom material '{}' with shaders: {} and {}",
		name, vertexShaderPath, fragmentShaderPath);
	return material;
}

std::shared_ptr<WireframeMaterial> MaterialManager::createWireframeMaterial(const std::string &name) {
	/// Check if material already exists
	auto it = this->materials.find(name);
	if (it != this->materials.end()) {
		/// Try to cast existing material to PBRMaterial
		auto wireframeMaterial = std::dynamic_pointer_cast<WireframeMaterial>(it->second);
		if (wireframeMaterial) {
			spdlog::debug("Returning existing Wireframe material '{}'", name);
			return wireframeMaterial;
		}

		/// Material exists but is not a PBR material
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Material '" + name + "' exists but is not a Wireframe material",
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	/// Create new Wireframe material
	auto material = std::make_shared<WireframeMaterial>(this->device, name, this->physicalDevice);

	/// Store in material map
	this->materials[name] = material;

	spdlog::info("Created new Wireframe material '{}'", name);
	return material;
}
std::shared_ptr<TerrainMaterial> MaterialManager::createTerrainMaterial(const std::string &name) {
	/// Check if material already exists
	auto it = this->materials.find(name);
	if (it != this->materials.end()) {
		/// Try to cast existing material to PBRMaterial
		auto terrainMaterial = std::dynamic_pointer_cast<TerrainMaterial>(it->second);
		if (terrainMaterial) {
			spdlog::debug("Returning existing Terrain material '{}'", name);
			return terrainMaterial;
		}

		/// Material exists but is not a PBR material
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Material '" + name + "' exists but is not a Terrain material",
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	/// Create new Terrain material
	auto material = std::make_shared<TerrainMaterial>(this->device, name, this->physicalDevice);

	/// Store in material map
	this->materials[name] = material;

	spdlog::info("Created new Terrain material '{}'", name);
	return material;
}

std::shared_ptr<DebugMaterial> MaterialManager::createDebugMaterial(const std::string& name) {
	/// Check if material already exists
	auto it = this->materials.find(name);
	if (it != this->materials.end()) {
		/// Try to cast existing material to DebugMaterial
		auto debugMaterial = std::dynamic_pointer_cast<DebugMaterial>(it->second);
		if (debugMaterial) {
			spdlog::debug("Returning existing Debug material '{}'", name);
			return debugMaterial;
		}
		
		/// Material exists but is not a Debug material
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Material '" + name + "' exists but is not a Debug material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create new Debug material
	auto material = std::make_shared<DebugMaterial>(
		this->device,
		name,
		this->physicalDevice
	);
	
	/// Store in material map
	this->materials[name] = material;

	spdlog::info("Created new Debug material '{}'", name);
	return material;
}

std::shared_ptr<SnowMaterial> MaterialManager::createSnowMaterial(const std::string& name) {
	/// Check if material already exists
	auto it = this->materials.find(name);
	if (it != this->materials.end()) {
		/// Try to cast existing material to SnowMaterial
		auto snowMaterial = std::dynamic_pointer_cast<SnowMaterial>(it->second);
		if (snowMaterial) {
			spdlog::debug("Returning existing Snow material '{}'", name);
			return snowMaterial;
		}

		/// Material exists but is not a Snow material
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Material '" + name + "' exists but is not a Snow material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create new Snow material
	auto material = std::make_shared<SnowMaterial>(
		this->device,
		name,
		this->physicalDevice
	);

	/// Store in material map
	this->materials[name] = material;

	/// Set default SSS LUT texture to avoid undefined binding
	/// This will be replaced with the actual SSS LUT later
	auto defaultTexture = this->textureManager->getDefaultTexture();
	material->setSSSLUT(defaultTexture);

	spdlog::info("Created new Snow material '{}'", name);
	return material;
}

std::shared_ptr<Material> MaterialManager::getMaterial(
	const std::string& name
) const {
	auto it = this->materials.find(name);
	if (it != this->materials.end()) {
		return it->second;
	}
	
	spdlog::debug("Material '{}' not found", name);
	return nullptr;
}

bool MaterialManager::hasMaterial(const std::string& name) const {
	return this->materials.find(name) != this->materials.end();
}

void MaterialManager::cleanup() {
	/// Clear the materials map
	/// This will trigger destruction of all materials
	/// thanks to shared_ptr reference counting
	size_t count = this->materials.size();
	this->materials.clear();
	
	if (count > 0) {
		spdlog::info("Cleaned up {} materials", count);
	}
}

void MaterialManager::validateMaterialName(const std::string& name) const {
	/// Check for empty name
	if (name.empty()) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Material name cannot be empty",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Check for existing material
	if (this->hasMaterial(name)) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Material '" + name + "' already exists",
			__FUNCTION__, __FILE__, __LINE__
		);
	}
}

} /// namespace lillugsi::rendering