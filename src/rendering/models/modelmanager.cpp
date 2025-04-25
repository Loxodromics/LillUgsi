#include "modelmanager.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <thread>
#include <chrono>

namespace lillugsi::rendering {

ModelManager::ModelManager(
	std::shared_ptr<MeshManager> meshManager,
	std::shared_ptr<MaterialManager> materialManager,
	std::shared_ptr<TextureManager> textureManager)
	: meshManager(std::move(meshManager))
	, materialManager(std::move(materialManager))
	, textureManager(std::move(textureManager)) {
	
	spdlog::info("Model manager initialized");
}

ModelManager::~ModelManager() {
	/// Wait for any pending async operations to complete
	/// This prevents potential crashes from async operations 
	/// trying to access the manager after it's destroyed
	this->waitForAsyncOperations();
	
	this->clearCache();
	spdlog::info("Model manager destroyed");
}

void ModelManager::registerLoader(std::shared_ptr<ModelLoader> loader) {
	/// Validate loader before adding
	if (!loader) {
		spdlog::warn("Attempted to register null model loader");
		return;
	}
	
	this->loaders.push_back(std::move(loader));
	spdlog::debug("Registered model loader, total loaders: {}", this->loaders.size());
}

bool ModelManager::initialize() {
	/// Register the built-in glTF loader
	/// This loader handles both .gltf and .glb formats
	try {
		auto gltfLoader = std::make_shared<GltfModelLoader>(
			this->meshManager,
			this->materialManager,
			this->textureManager
		);
		
		this->registerLoader(gltfLoader);
		spdlog::info("Registered glTF model loader");
		
		/// Add more loaders here as needed for other formats
		
		return true;
	}
	catch (const std::exception& e) {
		spdlog::error("Failed to initialize model manager: {}", e.what());
		return false;
	}
}

std::shared_ptr<scene::SceneNode> ModelManager::loadModel(
	const std::string& filePath,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode,
	const ModelLoadOptions& options) {
	
	/// Resolve and normalize path for cache lookup
	std::string resolvedPath = this->resolvePath(filePath);
	std::string normalizedPath = this->normalizePath(resolvedPath);
	
	/// Check if model is already in cache and loading is complete
	{
		std::lock_guard<std::mutex> lock(this->cacheMutex);
		
		auto it = this->modelCache.find(normalizedPath);
		if (it != this->modelCache.end() && it->second.isComplete) {
			/// If the cached model is still valid, we can use it
			if (auto cachedNode = it->second.rootNode.lock()) {
				spdlog::debug("Using cached model: {}", normalizedPath);
				
				/// If we want a separate instance with a different parent,
				/// we need to clone the hierarchy
				if (parentNode) {
					/// Clone the cached model to create a new instance
					auto newInstance = this->cloneNodeHierarchy(
						cachedNode, scene, parentNode);
					
					return newInstance;
				}
				
				/// Otherwise, return the cached model directly
				return cachedNode;
			}
			
			/// If the cached node is no longer valid, remove it from cache
			spdlog::debug("Cached model node expired, removing from cache: {}", normalizedPath);
			this->modelCache.erase(it);
		}
		
		/// Also check if the model is currently being loaded asynchronously
		/// This prevents starting a new load for a model that's already in progress
		if (this->isLoadingAsync(normalizedPath)) {
			spdlog::warn("Model '{}' is currently being loaded asynchronously", normalizedPath);
			return nullptr;
		}
	}
	
	/// Find an appropriate loader for this file
	auto loader = this->findLoader(normalizedPath);
	if (!loader) {
		spdlog::error("No suitable loader found for model: {}", normalizedPath);
		return nullptr;
	}
	
	/// Load the model
	spdlog::info("Loading model: {}", normalizedPath);
	auto modelNode = loader->loadModel(normalizedPath, scene, parentNode, options);
	
	/// Cache the loaded model if successful
	if (modelNode) {
		std::lock_guard<std::mutex> lock(this->cacheMutex);
		
		CachedModel cachedModel;
		cachedModel.rootNode = modelNode;
		cachedModel.filePath = normalizedPath;
		cachedModel.isComplete = true;
		
		this->modelCache[normalizedPath] = std::move(cachedModel);
		spdlog::info("Model cached: {}", normalizedPath);
	}
	
	return modelNode;
}

std::future<std::shared_ptr<scene::SceneNode>> ModelManager::loadModelAsync(
	const std::string& filePath,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode,
	const ModelLoadOptions& options) {
	
	/// Resolve and normalize path for cache lookup
	std::string resolvedPath = this->resolvePath(filePath);
	std::string normalizedPath = this->normalizePath(resolvedPath);
	
	/// Check if model is already in cache and loading is complete
	{
		std::lock_guard<std::mutex> lock(this->cacheMutex);
		
		auto it = this->modelCache.find(normalizedPath);
		if (it != this->modelCache.end() && it->second.isComplete) {
			/// If the cached model is still valid, we can use it
			if (auto cachedNode = it->second.rootNode.lock()) {
				spdlog::debug("Using cached model for async request: {}", normalizedPath);
				
				/// If we want a separate instance with a different parent,
				/// we need to clone the hierarchy
				if (parentNode) {
					/// Create an already-fulfilled future with a clone
					/// This makes the API consistent even for cached models
					std::promise<std::shared_ptr<scene::SceneNode>> promise;
					promise.set_value(this->cloneNodeHierarchy(
						cachedNode, scene, parentNode));
					return promise.get_future();
				}
				
				/// Return an already-fulfilled future with the cached node
				std::promise<std::shared_ptr<scene::SceneNode>> promise;
				promise.set_value(cachedNode);
				return promise.get_future();
			}
			
			/// If the cached node is no longer valid, remove it from cache
			spdlog::debug("Cached model node expired, removing from cache: {}", normalizedPath);
			this->modelCache.erase(it);
		}
		
		/// Check if the model is already being loaded asynchronously
		/// In this case, we should avoid starting a duplicate load
		for (const auto& op : this->asyncOperations) {
			if (op.filePath == normalizedPath) {
				spdlog::warn("Model '{}' is already being loaded asynchronously", normalizedPath);
				
				/// Create a promise that we can fulfill later
				/// This gives the caller a future to wait on even though we're not starting a new load
				std::promise<std::shared_ptr<scene::SceneNode>> promise;
				promise.set_value(nullptr);
				return promise.get_future();
			}
		}
	}
	
	/// First clean up any completed async operations
	this->cleanupCompletedAsyncOperations();
	
	/// Find an appropriate loader for this file
	auto loader = this->findLoader(normalizedPath);
	if (!loader) {
		spdlog::error("No suitable loader found for async model: {}", normalizedPath);
		
		/// Return a future that resolves to nullptr
		std::promise<std::shared_ptr<scene::SceneNode>> promise;
		promise.set_value(nullptr);
		return promise.get_future();
	}
	
	/// Add entry to model cache to indicate loading has started
	{
		std::lock_guard<std::mutex> lock(this->cacheMutex);
		
		CachedModel cachedModel;
		cachedModel.filePath = normalizedPath;
		cachedModel.isComplete = false; /// Mark as incomplete until async load finishes
		
		this->modelCache[normalizedPath] = std::move(cachedModel);
	}
	
	/// Start loading in a separate thread
	/// We use std::async for automatic thread management
	spdlog::info("Starting async load of model: {}", normalizedPath);
	auto future = std::async(std::launch::async, [this, loader, normalizedPath, &scene, parentNode, options]() {
		/// Load the model on a background thread
		auto modelNode = loader->loadModel(normalizedPath, scene, parentNode, options);
		
		/// Update cache when loading completes
		if (modelNode) {
			std::lock_guard<std::mutex> lock(this->cacheMutex);
			
			auto it = this->modelCache.find(normalizedPath);
			if (it != this->modelCache.end()) {
				it->second.rootNode = modelNode;
				it->second.isComplete = true;
				spdlog::info("Async model load complete and cached: {}", normalizedPath);
			}
		} else {
			/// Remove failed loads from cache
			std::lock_guard<std::mutex> lock(this->cacheMutex);
			this->modelCache.erase(normalizedPath);
			spdlog::error("Async model load failed: {}", normalizedPath);
		}
		
		return modelNode;
	});
	
	/// Track the async operation
	{
		std::lock_guard<std::mutex> lock(this->asyncMutex);
		
		AsyncLoadOperation operation;
		operation.future = future.share(); /// Use shared_future so we can track it
		operation.filePath = normalizedPath;
		
		this->asyncOperations.push_back(std::move(operation));
	}
	
	return future;
}

bool ModelManager::isLoadingAsync(const std::string& filePath) const {
	std::string normalizedPath = this->normalizePath(this->resolvePath(filePath));
	
	std::lock_guard<std::mutex> lock(this->asyncMutex);
	
	for (const auto& op : this->asyncOperations) {
		if (op.filePath == normalizedPath) {
			/// Check if the future is ready
			/// If it's still running, the model is being loaded
			auto status = op.future.wait_for(std::chrono::seconds(0));
			return status != std::future_status::ready;
		}
	}
	
	return false;
}

void ModelManager::waitForAsyncOperations() {
	std::lock_guard<std::mutex> lock(this->asyncMutex);
	
	if (!this->asyncOperations.empty()) {
		spdlog::info("Waiting for {} async model loading operations to complete", 
			this->asyncOperations.size());
		
		/// Wait for all futures to complete
		for (auto& op : this->asyncOperations) {
			op.future.wait();
		}
		
		/// Clear the list
		this->asyncOperations.clear();
		
		spdlog::info("All async model loading operations completed");
	}
}

bool ModelManager::isModelLoaded(const std::string& filePath) const {
	std::string normalizedPath = this->normalizePath(this->resolvePath(filePath));
	
	std::lock_guard<std::mutex> lock(this->cacheMutex);
	
	auto it = this->modelCache.find(normalizedPath);
	if (it != this->modelCache.end() && it->second.isComplete) {
		/// Check if the weak pointer still points to a valid object
		return !it->second.rootNode.expired();
	}
	
	return false;
}

std::shared_ptr<scene::SceneNode> ModelManager::instantiateModel(
	const std::string& filePath,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode) {
	
	/// Ensure parent node exists
	if (!parentNode) {
		parentNode = scene.getRoot();
	}
	
	std::string normalizedPath = this->normalizePath(this->resolvePath(filePath));
	
	/// Find the model in the cache
	std::shared_ptr<scene::SceneNode> sourceNode;
	
	{
		std::lock_guard<std::mutex> lock(this->cacheMutex);
		
		auto it = this->modelCache.find(normalizedPath);
		if (it != this->modelCache.end() && it->second.isComplete) {
			sourceNode = it->second.rootNode.lock();
			
			if (!sourceNode) {
				/// Model expired, remove from cache
				this->modelCache.erase(it);
				spdlog::debug("Cached model expired during instantiation: {}", normalizedPath);
				return nullptr;
			}
		} else {
			spdlog::warn("Attempted to instantiate model that isn't loaded: {}", normalizedPath);
			return nullptr;
		}
	}
	
	/// Create a new instance by cloning the hierarchy
	return this->cloneNodeHierarchy(sourceNode, scene, parentNode);
}

bool ModelManager::unloadModel(const std::string& filePath) {
	std::string normalizedPath = this->normalizePath(this->resolvePath(filePath));
	
	/// Check if the model is currently being loaded
	if (this->isLoadingAsync(normalizedPath)) {
		spdlog::warn("Cannot unload model '{}' while it's being loaded asynchronously", 
			normalizedPath);
		return false;
	}
	
	/// Remove from cache
	std::lock_guard<std::mutex> lock(this->cacheMutex);
	
	auto it = this->modelCache.find(normalizedPath);
	if (it != this->modelCache.end()) {
		/// Log whether the model is still in use
		bool expired = it->second.rootNode.expired();
		spdlog::debug("Unloading model {}: {}", 
			normalizedPath, expired ? "already expired" : "still in use");
		
		this->modelCache.erase(it);
		return true;
	}
	
	return false;
}

void ModelManager::clearCache() {
	/// First, wait for any pending async operations
	this->waitForAsyncOperations();
	
	std::lock_guard<std::mutex> lock(this->cacheMutex);
	
	size_t count = this->modelCache.size();
	if (count > 0) {
		spdlog::info("Clearing model cache with {} entries", count);
		
		/// Log details about each cached model
		size_t expired = 0;
		for (const auto& [path, model] : this->modelCache) {
			if (model.rootNode.expired()) {
				expired++;
			}
		}
		
		spdlog::debug("Model cache contained {} expired entries", expired);
		this->modelCache.clear();
	}
}

void ModelManager::setResourceBaseDirectory(const std::string& directory) {
	/// Store the base directory for resolving relative paths
	this->resourceBaseDirectory = directory;
	
	/// Ensure the directory has a trailing slash if it's not empty
	if (!this->resourceBaseDirectory.empty() && 
		this->resourceBaseDirectory.back() != '/' && 
		this->resourceBaseDirectory.back() != '\\') {
		this->resourceBaseDirectory += '/';
	}
	
	spdlog::info("Model resource base directory set to: '{}'", this->resourceBaseDirectory);
}

const std::string& ModelManager::getResourceBaseDirectory() const {
	return this->resourceBaseDirectory;
}

std::shared_ptr<ModelLoader> ModelManager::findLoader(const std::string& filePath) const {
	/// Extract file extension
	std::filesystem::path path(filePath);
	std::string extension = path.extension().string();
	
	/// Look for a loader that supports this extension
	for (const auto& loader : this->loaders) {
		if (loader->supportsFormat(extension)) {
			return loader;
		}
	}
	
	return nullptr;
}

std::string ModelManager::normalizePath(const std::string& filePath) const {
	/// Normalize paths to ensure consistent lookup regardless of format
	/// This converts paths like "models/../models/character.gltf" to "models/character.gltf"
	try {
		std::filesystem::path fsPath = std::filesystem::absolute(filePath).lexically_normal();
		return fsPath.string();
	}
	catch (const std::exception& e) {
		/// If path normalization fails, return the original path
		spdlog::warn("Failed to normalize path '{}': {}", filePath, e.what());
		return filePath;
	}
}

std::string ModelManager::resolvePath(const std::string& filePath) const {
	/// If the path is already absolute, return it as is
	if (std::filesystem::path(filePath).is_absolute()) {
		return filePath;
	}
	
	/// If we have a base directory, prepend it to the path
	if (!this->resourceBaseDirectory.empty()) {
		return this->resourceBaseDirectory + filePath;
	}
	
	/// Otherwise, return the path as is
	return filePath;
}

std::shared_ptr<scene::SceneNode> ModelManager::cloneNodeHierarchy(
	const std::shared_ptr<scene::SceneNode>& sourceNode,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode) const {
	
	if (!sourceNode) {
		return nullptr;
	}
	
	/// Create a new node with the same name
	auto newNode = scene.createNode(sourceNode->getName(), parentNode);
	
	/// Copy the transformation
	newNode->setLocalTransform(sourceNode->getLocalTransform());
	
	/// Copy the mesh if any
	/// Note: We reuse the same mesh instance, not clone it
	if (auto mesh = sourceNode->getMesh()) {
		newNode->setMesh(mesh);
	}
	
	/// Recursively clone children
	for (const auto& child : sourceNode->getChildren()) {
		auto childClone = this->cloneNodeHierarchy(child, scene, newNode);
	}
	
	return newNode;
}

void ModelManager::cleanupCompletedAsyncOperations() {
	std::lock_guard<std::mutex> lock(this->asyncMutex);
	
	/// Remove operations whose futures have completed
	auto it = this->asyncOperations.begin();
	while (it != this->asyncOperations.end()) {
		/// Check if the future is ready without blocking
		auto status = it->future.wait_for(std::chrono::seconds(0));
		
		if (status == std::future_status::ready) {
			/// This operation is complete, remove it
			it = this->asyncOperations.erase(it);
		} else {
			/// Move to next operation
			++it;
		}
	}
}

} /// namespace lillugsi::rendering