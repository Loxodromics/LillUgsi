#pragma once

#include "modelloader.h"
#include "rendering/meshmanager.h"
#include "rendering/materialmanager.h"
#include "rendering/texturemanager.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

namespace lillugsi::rendering {

/// ModelManager centralizes model loading and resource management
/// We use this to:
/// 1. Coordinate different model loaders for various formats
/// 2. Manage caching of loaded models
/// 3. Handle model resource lifecycle
class ModelManager {
public:
	/// Create a model manager
	/// @param meshManager Manager for creating and managing meshes
	/// @param materialManager Manager for creating and managing materials
	/// @param textureManager Manager for loading and managing textures
	ModelManager(
		std::shared_ptr<MeshManager> meshManager,
		std::shared_ptr<MaterialManager> materialManager,
		std::shared_ptr<TextureManager> textureManager);
		
	/// Destructor ensures proper cleanup
	~ModelManager();
	
	/// Register a model loader for a specific format
	/// @param loader The loader to register
	void registerLoader(std::shared_ptr<ModelLoader> loader);
	
	/// Load a model from file
	/// If the model has been loaded before, returns the cached version
	/// @param filePath Path to the model file
	/// @param scene Scene to load the model into
	/// @param parentNode Parent node to attach the model to (optional)
	/// @param options Options controlling loading behavior
	/// @return Root node of the loaded model, or nullptr if loading failed
	[[nodiscard]] std::shared_ptr<scene::SceneNode> loadModel(
		const std::string& filePath,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode = nullptr,
		const ModelLoadOptions& options = ModelLoadOptions());
		
	/// Check if a model is already loaded
	/// @param filePath Path to the model file
	/// @return True if the model is already loaded
	[[nodiscard]] bool isModelLoaded(const std::string& filePath) const;
	
	/// Clear the model cache
	/// This releases all cached models that aren't referenced elsewhere
	/// Useful for freeing memory between levels or during low-memory situations
	void clearCache();
	
private:
	/// Find an appropriate loader for the given file
	/// @param filePath Path to the model file
	/// @return Suitable loader, or nullptr if none found
	[[nodiscard]] std::shared_ptr<ModelLoader> findLoader(const std::string& filePath) const;
	
	/// Normalize a file path for consistent cache lookups
	/// @param filePath Path to normalize
	/// @return Normalized path
	[[nodiscard]] std::string normalizePath(const std::string& filePath) const;

	std::shared_ptr<MeshManager> meshManager;         /// For creating mesh resources
	std::shared_ptr<MaterialManager> materialManager; /// For creating materials
	std::shared_ptr<TextureManager> textureManager;   /// For loading textures
	
	/// Available loaders for different formats
	std::vector<std::shared_ptr<ModelLoader>> loaders;
	
	/// Cache of loaded models
	struct CachedModel {
		std::weak_ptr<scene::SceneNode> rootNode; /// Weak reference to allow cleanup
		std::string filePath;                     /// Original file path
	};
	std::unordered_map<std::string, CachedModel> modelCache;
	
	/// Mutex for thread-safe model cache access
	mutable std::mutex cacheMutex;
};

} /// namespace lillugsi::rendering