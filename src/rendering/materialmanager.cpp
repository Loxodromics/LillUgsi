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

	/// Set default textures for all material slots, since they are all need when binding
	/// They might be overwriten with propper textures later
	auto defaultTexture = this->textureManager->getDefaultTexture();
	material->setAlbedoTexture(defaultTexture);
	material->setNormalMap(defaultTexture);
	material->setRoughnessMap(defaultTexture);
	material->setMetallicMap(defaultTexture);
	material->setOcclusionMap(defaultTexture);

	spdlog::info("Created new PBR material '{}'", name);
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