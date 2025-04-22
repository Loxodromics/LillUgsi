#include "modelmanager.h"
#include <spdlog/spdlog.h>
#include <filesystem>

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

std::shared_ptr<scene::SceneNode> ModelManager::loadModel(
	const std::string& filePath,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode,
	const ModelLoadOptions& options) {
	
	/// Normalize path for cache lookup
	std::string normalizedPath = this->normalizePath(filePath);
	
	/// Check if model is already loaded
	{
		std::lock_guard<std::mutex> lock(this->cacheMutex);
		
		auto it = this->modelCache.find(normalizedPath);
		if (it != this->modelCache.end()) {
			/// If the cached root node is still valid, use it
			if (auto cachedNode = it->second.rootNode.lock()) {
				spdlog::debug("Using cached model: {}", normalizedPath);
				
				/// If parent node provided, create a new instance
				if (parentNode) {
					/// Create a new node as a copy of the cached model
					/// This allows for multiple instances of the same model
					auto newNode = scene.createNode(
						std::filesystem::path(normalizedPath).stem().string(),
						parentNode);
					
					/// TODO: Clone the cached model's structure to the new node
					/// For now, just return the cached node
					return cachedNode;
				}
				
				return cachedNode;
			}
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
		
		this->modelCache[normalizedPath] = std::move(cachedModel);
		spdlog::info("Model cached: {}", normalizedPath);
	}
	
	return modelNode;
}

bool ModelManager::isModelLoaded(const std::string& filePath) const {
	std::string normalizedPath = this->normalizePath(filePath);
	std::lock_guard<std::mutex> lock(this->cacheMutex);
	
	auto it = this->modelCache.find(normalizedPath);
	if (it != this->modelCache.end()) {
		/// Check if the weak pointer still points to a valid object
		return !it->second.rootNode.expired();
	}
	
	return false;
}

void ModelManager::clearCache() {
	std::lock_guard<std::mutex> lock(this->cacheMutex);
	
	/// Count valid and expired models for logging
	size_t validCount = 0;
	size_t expiredCount = 0;
	
	for (const auto& [path, model] : this->modelCache) {
		if (model.rootNode.expired()) {
			expiredCount++;
		} else {
			validCount++;
		}
	}
	
	/// Clear the cache
	this->modelCache.clear();
	
	spdlog::info("Model cache cleared. Valid: {}, Expired: {}", validCount, expiredCount);
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

} /// namespace lillugsi::rendering