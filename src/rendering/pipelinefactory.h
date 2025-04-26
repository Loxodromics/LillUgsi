#pragma once

#include "models/modeldata.h"
#include "vulkan/pipelinemanager.h"
#include "rendering/materialmanager.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace lillugsi::rendering {

/// PipelineFactory manages the creation of rendering pipelines for model materials
/// We separate this from the model loading to maintain separation of concerns
/// and allow for different pipeline creation strategies without modifying the loaders
class PipelineFactory {
public:
	/// Create a pipeline factory
	/// @param pipelineManager The pipeline manager to use for pipeline creation
	/// @param materialManager The material manager to use for material retrieval/creation
	PipelineFactory(
		std::shared_ptr<vulkan::PipelineManager> pipelineManager,
		std::shared_ptr<MaterialManager> materialManager);
	
	/// Destructor ensures proper cleanup
	~PipelineFactory() = default;
	
	/// Create pipelines for all materials in a model
	/// This processes each material in the model data and ensures
	/// appropriate pipelines are created and cached.
	/// @param modelData The model data containing materials to process
	/// @return True if all pipelines were created successfully
	[[nodiscard]] bool createPipelinesForModel(const ModelData& modelData);
	
	/// Create a pipeline for a specific material
	/// We expose this method to allow manual pipeline creation
	/// for materials that aren't directly part of a model
	/// @param materialName The name of the material to create a pipeline for
	/// @return True if the pipeline was created successfully
	[[nodiscard]] bool createPipelineForMaterial(const std::string& materialName);
	
	/// Check if a pipeline exists for a material
	/// @param materialName The name of the material to check
	/// @return True if a pipeline exists for the material
	[[nodiscard]] bool hasPipeline(const std::string& materialName) const;
	
	/// Clear the pipeline cache
	/// This releases all pipeline handles, allowing their resources to be freed
	/// Note: This won't destroy pipelines that are still in use by active materials
	void clearCache();
	
private:
	/// Extract specific material features for pipeline configuration
	/// We analyze the material properties to determine which pipeline features
	/// need to be enabled, such as alpha blending, double-sided rendering, etc.
	/// @param materialInfo The material info to analyze
	/// @param material The material to configure
	/// @return True if all features were successfully applied
	[[nodiscard]] bool configureMaterialFeatures(
		const ModelData::MaterialInfo& materialInfo,
		std::shared_ptr<Material> material);
	
	/// Create standard material parameters from model data
	/// We map generic material properties from the model data
	/// to our engine's specific material parameters
	/// @param materialInfo Source material data from the model
	/// @param material Target material to configure
	/// @return True if parameters were successfully applied
	[[nodiscard]] bool setStandardMaterialParams(
		const ModelData::MaterialInfo& materialInfo,
		std::shared_ptr<PBRMaterial> material);
	
	/// Reference to the pipeline manager
	/// We don't own this, just use it for creating pipelines
	std::shared_ptr<vulkan::PipelineManager> pipelineManager;
	
	/// Reference to the material manager
	/// We don't own this, just use it for creating/retrieving materials
	std::shared_ptr<MaterialManager> materialManager;
	
	/// Cache of created pipelines for quick lookup
	/// Maps material names to a simple boolean indicating pipeline existence
	/// We don't need to store the actual pipeline as it's managed by PipelineManager
	std::unordered_map<std::string, bool> pipelineCache;
};

} /// namespace lillugsi::rendering