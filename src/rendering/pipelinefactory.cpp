#include "pipelinefactory.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

PipelineFactory::PipelineFactory(
	std::shared_ptr<vulkan::PipelineManager> pipelineManager,
	std::shared_ptr<MaterialManager> materialManager)
	: pipelineManager(std::move(pipelineManager))
	, materialManager(std::move(materialManager)) {
	
	spdlog::info("Pipeline factory created");
}

bool PipelineFactory::createPipelinesForModel(const ModelData& modelData) {
	bool success = true;
	
	/// Log the pipeline creation process
	spdlog::debug("Creating pipelines for model '{}' with {} materials", 
		modelData.name, modelData.materials.size());
	
	/// Process each material in the model
	/// We create pipelines for each material to ensure all rendering variations are supported
	for (const auto& [name, materialInfo] : modelData.materials) {
		/// Check if we've already created a pipeline for this material
		/// This prevents duplicate pipeline creation for shared materials
		if (this->hasPipeline(name)) {
			spdlog::debug("Pipeline for material '{}' already exists", name);
			continue;
		}
		
		/// Get the material from the material manager if it exists, or create it if not
		/// We need to handle both pre-existing materials and new ones from the model
		std::shared_ptr<Material> material;
		if (this->materialManager->hasMaterial(name)) {
			material = this->materialManager->getMaterial(name);
		} else {
			/// For model materials that don't exist yet, we create a PBR material
			/// PBR is our standard material type for imported models
			auto pbrMaterial = this->materialManager->createPBRMaterial(name);
			
			/// Configure the material with properties from the model
			if (!this->setStandardMaterialParams(materialInfo, pbrMaterial)) {
				spdlog::warn("Failed to fully configure material '{}'", name);
				/// Continue anyway - partial configuration is better than none
			}
			
			material = pbrMaterial;
		}
		
		/// Apply material-specific features (transparency, double-sided, etc.)
		/// These features affect the pipeline configuration
		if (!this->configureMaterialFeatures(materialInfo, material)) {
			spdlog::warn("Failed to configure some features for material '{}'", name);
			/// Continue anyway with best-effort configuration
		}
		
		/// Create the pipeline for this material
		/// This is the core operation where the actual Vulkan pipeline is created
		try {
			auto pipeline = this->pipelineManager->createPipeline(*material);
			if (pipeline) {
				/// Cache the successful pipeline creation
				this->pipelineCache[name] = true;
				spdlog::info("Created pipeline for material '{}'", name);
			} else {
				/// Record failure but continue with other materials
				spdlog::error("Failed to create pipeline for material '{}'", name);
				success = false;
			}
		} catch (const vulkan::VulkanException& e) {
			/// Handle Vulkan errors during pipeline creation
			spdlog::error("Pipeline creation error for material '{}': {}", name, e.what());
			success = false;
		} catch (const std::exception& e) {
			/// Handle any other unexpected errors
			spdlog::error("Unexpected error creating pipeline for material '{}': {}", name, e.what());
			success = false;
		}
	}
	
	/// Return overall success status
	/// Even partial success is considered valid, as we want to render what we can
	return success;
}

bool PipelineFactory::createPipelineForMaterial(const std::string& materialName) {
	/// Check if we already have a pipeline for this material
	/// This prevents duplicate work and resource allocation
	if (this->hasPipeline(materialName)) {
		spdlog::debug("Pipeline for material '{}' already exists", materialName);
		return true;
	}
	
	/// Get the material from the material manager
	/// We require the material to exist already for this method
	auto material = this->materialManager->getMaterial(materialName);
	if (!material) {
		spdlog::error("Cannot create pipeline for non-existent material '{}'", materialName);
		return false;
	}
	
	/// Create the pipeline
	/// This delegates to the pipeline manager which handles the actual Vulkan work
	try {
		auto pipeline = this->pipelineManager->createPipeline(*material);
		if (pipeline) {
			/// Cache the successful pipeline creation
			this->pipelineCache[materialName] = true;
			spdlog::info("Created pipeline for material '{}'", materialName);
			return true;
		} else {
			spdlog::error("Failed to create pipeline for material '{}'", materialName);
			return false;
		}
	} catch (const vulkan::VulkanException& e) {
		/// Handle Vulkan errors during pipeline creation
		spdlog::error("Pipeline creation error for material '{}': {}", materialName, e.what());
		return false;
	}
}

bool PipelineFactory::hasPipeline(const std::string& materialName) const {
	/// First check our local cache for quick lookup
	/// This avoids needing to query the pipeline manager for common cases
	auto it = this->pipelineCache.find(materialName);
	if (it != this->pipelineCache.end()) {
		return it->second;
	}
	
	/// If not in our cache, check with the pipeline manager
	/// This handles pipelines that might have been created elsewhere
	bool exists = this->pipelineManager->hasPipeline(materialName);
	
	/// Update our cache for future queries
	/// This is logically const as it doesn't change observable behavior
	const_cast<PipelineFactory*>(this)->pipelineCache[materialName] = exists;
	
	return exists;
}

void PipelineFactory::clearCache() {
	/// Clear our local pipeline existence cache
	/// We don't own the pipelines themselves, just track their existence
	size_t count = this->pipelineCache.size();
	this->pipelineCache.clear();
	
	spdlog::debug("Cleared pipeline factory cache ({} entries)", count);
}

bool PipelineFactory::configureMaterialFeatures(
	const ModelData::MaterialInfo& materialInfo,
	std::shared_ptr<Material> material) {
	
	/// Currently our Material class doesn't allow directly configuring features after creation
	/// In a future enhancement, we could add methods like setDoubleSided(), setTransparent(), etc.
	
	/// For now, we'll log which features we would apply
	/// This serves as documentation for future implementation
	
	if (materialInfo.doubleSided) {
		spdlog::debug("Material '{}' is double-sided (feature not directly configurable yet)", 
			material->getName());
		/// Future: material->setDoubleSided(true);
	}
	
	if (materialInfo.transparent) {
		spdlog::debug("Material '{}' is transparent (feature not directly configurable yet)", 
			material->getName());
		/// Future: material->setTransparent(true);
		
		/// Log alpha mode information
		switch (materialInfo.alphaMode) {
			case ModelData::MaterialInfo::AlphaMode::Blend:
				spdlog::debug("Material '{}' uses alpha blending", material->getName());
				break;
			case ModelData::MaterialInfo::AlphaMode::Mask:
				spdlog::debug("Material '{}' uses alpha masking with cutoff {}", 
					material->getName(), materialInfo.alphaCutoff);
				break;
			default:
				break;
		}
	}
	
	if (materialInfo.unlit) {
		spdlog::debug("Material '{}' is unlit (feature not directly configurable yet)", 
			material->getName());
		/// Future: material->setUnlit(true);
	}
	
	/// Return true as we've done our best with the current implementation
	return true;
}

bool PipelineFactory::setStandardMaterialParams(
	const ModelData::MaterialInfo& materialInfo,
	std::shared_ptr<PBRMaterial> material) {
	
	/// Set base color with alpha
	/// This is the diffuse/albedo color of the material
	material->setBaseColor(materialInfo.baseColor);
	
	/// Set PBR metallic property 
	/// Controls how metallic vs. dielectric the surface appears
	material->setMetallic(materialInfo.metallic);
	
	/// Set PBR roughness property
	/// Controls microfacet distribution - how rough/smooth the surface appears
	material->setRoughness(materialInfo.roughness);
	
	/// Set ambient occlusion factor
	/// Controls how much ambient light is occluded in crevices
	material->setAmbient(materialInfo.occlusion);
	
	/// Set normal mapping strength if a normal map is present
	if (!materialInfo.normalTexturePath.empty()) {
		material->setNormalStrength(materialInfo.normalScale);
	}
	
	spdlog::debug("Configured material '{}' with base color=({},{},{},{}), metallic={}, roughness={}", 
		material->getName(),
		materialInfo.baseColor.r, 
		materialInfo.baseColor.g, 
		materialInfo.baseColor.b, 
		materialInfo.baseColor.a,
		materialInfo.metallic,
		materialInfo.roughness);
	
	/// The texture assignments would typically be done separately by the model loader
	/// since they require actual texture loading, not just parameter setting
	
	return true;
}

} /// namespace lillugsi::rendering