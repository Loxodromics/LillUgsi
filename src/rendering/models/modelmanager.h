#pragma once

#include "modelloader.h"
#include "gltfmodelloader.h"
#include "rendering/meshmanager.h"
#include "rendering/materialmanager.h"
#include "rendering/texturemanager.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <future>

namespace lillugsi::rendering {

/*
 * ModelManager centralizes all model loading and resource management operations.
 * 
 * This class serves as the primary interface for loading 3D models into the engine.
 * It handles format detection, resource caching, instantiation, and provides both
 * synchronous and asynchronous loading capabilities.
 * 
 * Asynchronous Loading:
 * The manager supports loading models in background threads via loadModelAsync(),
 * which returns a std::future that resolves when loading completes. This prevents
 * blocking the main thread during potentially lengthy model loading operations.
 * Background loading operations are tracked internally and can be monitored with
 * isLoadingAsync() or waited on with waitForAsyncOperations().
 * 
 * Resource Management:
 * Loaded models are cached by their filepath to prevent redundant loading.
 * The cache uses weak pointers to allow unused models to be garbage collected
 * while retaining quick access to frequently used models. The cache can be
 * explicitly managed via unloadModel() and clearCache().
 * 
 * Model Instantiation:
 * Models can be instantiated (cloned) via instantiateModel(), which creates
 * a new scene hierarchy while reusing the underlying mesh and material resources.
 * This allows efficient placement of multiple instances of the same model.
 * 
 * Format Support:
 * The manager uses a plugin-based approach with registered ModelLoader instances
 * to support different file formats. By default, it includes support for glTF
 * (.gltf and .glb) files, with the ability to add loaders for additional formats.
 * 
 * Path Resolution:
 * The manager handles both absolute and relative paths, with support for a
 * configurable resource base directory for resolving relative paths consistently.
 */
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
	
	/// Initialize the model manager with default loaders
	/// This registers standard loaders like glTF
	/// @return True if initialization was successful
	bool initialize();
	
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
		
	/// Begin loading a model asynchronously
	/// This loads the model in a background thread without blocking
	/// @param filePath Path to the model file
	/// @param scene Scene to load the model into
	/// @param parentNode Parent node to attach the model to (optional)
	/// @param options Options controlling loading behavior
	/// @return Future that will contain the root node when loading completes
	[[nodiscard]] std::future<std::shared_ptr<scene::SceneNode>> loadModelAsync(
		const std::string& filePath,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode = nullptr,
		const ModelLoadOptions& options = ModelLoadOptions());
		
	/// Check if a model is currently being loaded asynchronously
	/// @param filePath Path to the model file
	/// @return True if the model is being loaded asynchronously
	[[nodiscard]] bool isLoadingAsync(const std::string& filePath) const;
		
	/// Wait for all async loading operations to complete
	/// This is useful when preparing to change scenes or shutdown
	void waitForAsyncOperations();
	
	/// Check if a model is already loaded and cached
	/// @param filePath Path to the model file
	/// @return True if the model is already loaded
	[[nodiscard]] bool isModelLoaded(const std::string& filePath) const;
	
	/// Get a previously loaded model instance
	/// This creates a new instance using the cached model data
	/// @param filePath Path to the previously loaded model
	/// @param scene Scene to create the instance in
	/// @param parentNode Parent node to attach the instance to
	/// @return New instance of the model, or nullptr if not found
	[[nodiscard]] std::shared_ptr<scene::SceneNode> instantiateModel(
		const std::string& filePath,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode);
	
	/// Remove a model from the cache
	/// This won't affect existing instances in the scene
	/// @param filePath Path to the model to remove
	/// @return True if the model was found and removed
	bool unloadModel(const std::string& filePath);
	
	/// Clear the model cache
	/// This releases all cached models that aren't referenced elsewhere
	/// Useful for freeing memory between levels or during low-memory situations
	void clearCache();
	
	/// Set the base directory for model resources
	/// Relative paths will be resolved from this directory
	/// @param directory Base directory path
	void setResourceBaseDirectory(const std::string& directory);
	
	/// Get the resource base directory
	/// @return Current base directory for model resources
	[[nodiscard]] const std::string& getResourceBaseDirectory() const;
	
private:
	/// Find an appropriate loader for the given file
	/// @param filePath Path to the model file
	/// @return Suitable loader, or nullptr if none found
	[[nodiscard]] std::shared_ptr<ModelLoader> findLoader(const std::string& filePath) const;
	
	/// Normalize a file path for consistent cache lookups
	/// @param filePath Path to normalize
	/// @return Normalized path
	[[nodiscard]] std::string normalizePath(const std::string& filePath) const;
	
	/// Resolve a relative path using the resource base directory
	/// @param filePath Path to resolve
	/// @return Resolved absolute path
	[[nodiscard]] std::string resolvePath(const std::string& filePath) const;
	
	/// Clone a scene node hierarchy for instancing
	/// @param sourceNode Source node to clone
	/// @param scene Scene to create new nodes in
	/// @param parentNode Parent for the cloned hierarchy
	/// @return Root of the cloned hierarchy
	[[nodiscard]] std::shared_ptr<scene::SceneNode> cloneNodeHierarchy(
		const std::shared_ptr<scene::SceneNode>& sourceNode,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode) const;

	std::shared_ptr<MeshManager> meshManager;         /// For creating mesh resources
	std::shared_ptr<MaterialManager> materialManager; /// For creating materials
	std::shared_ptr<TextureManager> textureManager;   /// For loading textures
	
	/// Available loaders for different formats
	std::vector<std::shared_ptr<ModelLoader>> loaders;
	
	/// Base directory for resolving relative paths
	std::string resourceBaseDirectory;
	
	/// Cache of loaded models
	struct CachedModel {
		std::weak_ptr<scene::SceneNode> rootNode; /// Weak reference to allow cleanup
		std::string filePath;                     /// Original file path
		bool isComplete{false};                   /// Whether loading is complete
	};
	std::unordered_map<std::string, CachedModel> modelCache;
	
	/// Active async loading operations
	struct AsyncLoadOperation {
		std::shared_future<std::shared_ptr<scene::SceneNode>> future; // Change to shared_future
		std::string filePath;
	};
	std::vector<AsyncLoadOperation> asyncOperations;
	
	/// Mutex for thread-safe model cache access
	mutable std::mutex cacheMutex;
	
	/// Mutex for thread-safe async operations list
	mutable std::mutex asyncMutex;
	
	/// Clean up completed async operations
	/// This removes futures that have completed from the tracking list
	void cleanupCompletedAsyncOperations();
};

} /// namespace lillugsi::rendering