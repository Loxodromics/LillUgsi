#pragma once

#include "scene/scene.h"
#include <memory>
#include <string>

namespace lillugsi::rendering {

/// Options that control how models are loaded
/// These allow customizing model loading behavior without changing loader code
struct ModelLoadOptions {
	bool calculateTangents{true};   /// Whether to calculate tangent vectors for normal mapping
	bool generateMips{true};        /// Whether to generate mipmaps for textures
	bool loadAnimations{true};      /// Whether to load and process animations
	float scale{1.0f};              /// Global scale factor for the loaded model
};

/// Base interface for all model loaders
/// We use a common interface to support multiple model formats
/// while maintaining consistent loading behavior
class ModelLoader {
public:
	/// Virtual destructor ensures proper cleanup in derived classes
	virtual ~ModelLoader() = default;
	
	/// Load a model from the given file into the scene
	/// @param filePath Path to the model file
	/// @param scene Scene to load the model into
	/// @param parentNode Parent node to attach the model to (optional)
	/// @param options Options controlling loading behavior
	/// @return Root node of the loaded model, or nullptr if loading failed
	[[nodiscard]] virtual std::shared_ptr<scene::SceneNode> loadModel(
		const std::string& filePath,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode = nullptr,
		const ModelLoadOptions& options = ModelLoadOptions()) = 0;
		
	/// Check if this loader supports the given file format
	/// @param fileExtension The file extension to check
	/// @return True if this loader supports the format
	[[nodiscard]] virtual bool supportsFormat(const std::string& fileExtension) const = 0;
};

} /// namespace lillugsi::rendering