#include "scenegraphconstructor.h"
#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>

namespace lillugsi::rendering {

SceneGraphConstructor::SceneGraphConstructor(
	const tinygltf::Model& gltfModel,
	const ModelData& modelData,
	const std::vector<std::shared_ptr<Mesh>>& meshes)
	: gltfModel(gltfModel)
	, modelData(modelData)
	, meshes(meshes) {
}

std::shared_ptr<scene::SceneNode> SceneGraphConstructor::buildSceneGraph(
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode,
	const ModelLoadOptions& options) {
	
	/// Ensure we have a valid parent node, defaulting to scene root if none provided
	if (!parentNode) {
		parentNode = scene.getRoot();
	}
	
	/// Create a root node for the model to group all imported nodes
	auto modelRootNode = scene.createNode(this->modelData.name, parentNode);
	
	/// Process the scene hierarchy
	/// glTF files can specify a default scene, or we use the first one
	int sceneIndex = this->gltfModel.defaultScene >= 0 ? this->gltfModel.defaultScene : 0;
	
	if (sceneIndex >= 0 && sceneIndex < static_cast<int>(this->gltfModel.scenes.size())) {
		const auto& gltfScene = this->gltfModel.scenes[sceneIndex];
		
		/// Process each root node in the scene
		for (int nodeIndex : gltfScene.nodes) {
			auto childNode = this->processNode(nodeIndex, scene, modelRootNode, options);
			
			/// Store processed node in our map for later reference
			if (childNode) {
				this->nodeMap[nodeIndex] = childNode;
			}
		}
	} else if (!this->gltfModel.nodes.empty()) {
		/// If no valid scene is defined, process each root node directly
		/// Some glTF files don't specify scenes and just have nodes
		for (size_t i = 0; i < this->gltfModel.nodes.size(); ++i) {
			auto childNode = this->processNode(static_cast<int>(i), scene, modelRootNode, options);
			
			/// Store processed node in our map for later reference
			if (childNode) {
				this->nodeMap[static_cast<int>(i)] = childNode;
			}
		}
	}
	
	/// Update bounds for the entire hierarchy
	modelRootNode->updateBoundsIfNeeded();
	
	spdlog::info("Built scene graph with {} nodes", this->nodeMap.size());
	return modelRootNode;
}

std::shared_ptr<scene::SceneNode> SceneGraphConstructor::processNode(
	int nodeIndex,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode,
	const ModelLoadOptions& options) {
	
	/// Check if we've already processed this node
	auto it = this->nodeMap.find(nodeIndex);
	if (it != this->nodeMap.end()) {
		/// Node already exists, return it
		return it->second;
	}
	
	/// Validate node index
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(this->modelData.nodes.size())) {
		spdlog::warn("Invalid node index: {}", nodeIndex);
		return nullptr;
	}
	
	/// Get node data
	const auto& nodeInfo = this->modelData.nodes[nodeIndex];
	
	/// Create a new scene node
	auto sceneNode = scene.createNode(nodeInfo.name, parentNode);
	
	/// Apply transformations from the glTF node
	this->applyNodeTransform(nodeInfo, sceneNode, options.scale);
	
	/// Handle node's mesh if it has one
	bool hasMesh = this->assignNodeMesh(nodeInfo, sceneNode);
	
	/// If the node has a mesh with multiple primitives, create child nodes
	if (hasMesh && nodeInfo.meshIndex >= 0) {
		this->handlePrimitiveGroups(nodeInfo.meshIndex, nodeInfo.name, scene, sceneNode);
	}
	
	/// Process child nodes recursively
	for (int childIndex : nodeInfo.children) {
		auto childNode = this->processNode(childIndex, scene, sceneNode, options);
		
		/// Store child node in our map
		if (childNode) {
			this->nodeMap[childIndex] = childNode;
		}
	}
	
	return sceneNode;
}

void SceneGraphConstructor::applyNodeTransform(
	const ModelData::NodeInfo& nodeInfo,
	std::shared_ptr<scene::SceneNode> sceneNode,
	float scale) {
	
	/// Convert from the modelData representation to the scene node representation
	scene::Transform transform;
	
	/// Apply position
	transform.position = nodeInfo.translation;
	
	/// Apply rotation
	transform.rotation = nodeInfo.rotation;
	
	/// Apply scale, including the global scale factor
	transform.scale = nodeInfo.scale * scale;
	
	/// Set the node's local transform
	sceneNode->setLocalTransform(transform);
	
	spdlog::trace("Applied transform to node '{}': pos=({},{},{}), scale=({},{},{})",
		nodeInfo.name,
		transform.position.x, transform.position.y, transform.position.z,
		transform.scale.x, transform.scale.y, transform.scale.z);
}

bool SceneGraphConstructor::assignNodeMesh(
	const ModelData::NodeInfo& nodeInfo,
	std::shared_ptr<scene::SceneNode> sceneNode) {

	if (nodeInfo.meshIndex < 0 || nodeInfo.meshIndex >= static_cast<int>(this->meshes.size())) {
		spdlog::warn("Mesh index {} out of range for node '{}'", nodeInfo.meshIndex, nodeInfo.name);
		return false;
	}

	// Directly map the mesh index to our meshes array
	sceneNode->setMesh(this->meshes[nodeInfo.meshIndex]);

	// Add debug bounds reporting for this node
	auto mesh = this->meshes[nodeInfo.meshIndex];
	auto& vertices = mesh->getVertices();
	if (!vertices.empty()) {
		scene::BoundingBox localBounds;
		for (const auto& v : vertices) {
			localBounds.addPoint(v.position);
		}

		spdlog::info("Node '{}' mesh bounds: min=({},{},{}), max=({},{},{})",
			nodeInfo.name,
			localBounds.getMin().x, localBounds.getMin().y, localBounds.getMin().z,
			localBounds.getMax().x, localBounds.getMax().y, localBounds.getMax().z);
	}

	return true;
}

bool SceneGraphConstructor::handlePrimitiveGroups(
	int meshIndex,
	const std::string& nodeName,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode) {
	
	/// Validate mesh index
	if (meshIndex < 0 || meshIndex >= static_cast<int>(this->gltfModel.meshes.size())) {
		return false;
	}
	
	const auto& gltfMesh = this->gltfModel.meshes[meshIndex];
	
	/// If there's only one primitive, no need for child nodes
	if (gltfMesh.primitives.size() <= 1) {
		return false;
	}
	
	/// Multiple primitives - create child nodes for each one after the first
	/// The first primitive is already assigned to the parent node
	for (size_t primitiveIndex = 1; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex) {
		/// Create a child node for this primitive
		std::string primitiveName = nodeName + "_primitive_" + std::to_string(primitiveIndex);
		auto primitiveNode = scene.createNode(primitiveName, parentNode);
		
		/// Find the corresponding mesh for this primitive
		int meshDataIndex = -1;
		for (size_t i = 0; i < this->modelData.meshes.size(); ++i) {
			const auto& meshName = this->modelData.meshes[i].name;
			
			/// Check for meshes with the primitive index pattern
			if (meshName == gltfMesh.name + "_" + std::to_string(primitiveIndex) ||
				meshName == "mesh_" + std::to_string(meshIndex) + "_" + std::to_string(primitiveIndex)) {
				meshDataIndex = static_cast<int>(i);
				break;
			}
		}
		
		/// Assign mesh if found
		if (meshDataIndex >= 0 && meshDataIndex < static_cast<int>(this->meshes.size())) {
			primitiveNode->setMesh(this->meshes[meshDataIndex]);
			spdlog::debug("Assigned mesh '{}' to primitive node '{}'", 
				this->modelData.meshes[meshDataIndex].name, primitiveName);
		} else {
			spdlog::warn("No matching mesh found for primitive {} of mesh '{}'", 
				primitiveIndex, gltfMesh.name);
		}
	}
	
	return true;
}

} /// namespace lillugsi::rendering