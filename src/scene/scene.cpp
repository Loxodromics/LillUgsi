#include "scene/scene.h"
#include <spdlog/spdlog.h>

namespace lillugsi::scene {

Scene::Scene()
	: nodeCount(0)
	, needsFullUpdate(true) {
	this->initialize();
	spdlog::info("Scene created with root node");
}

void Scene::initialize() {
	/// Create root node first as it serves as the foundation for the scene graph
	this->root = std::make_shared<SceneNode>("Root");
	this->nodeCount = 1;

	/// Create a dedicated terrain root node
	/// We separate terrain nodes to allow for specialized handling of terrain chunks
	this->terrainRoot = std::make_shared<SceneNode>("TerrainRoot");
	this->root->addChild(this->terrainRoot);
	this->nodeCount++;

	spdlog::info("Scene initialized with root nodes");
}

std::shared_ptr<SceneNode> Scene::createNode(const std::string& name, std::shared_ptr<SceneNode> parent) {
	/// Create the new node
	auto node = std::make_shared<SceneNode>(name);
	this->nodeCount++;

	/// If no parent is specified, we attach to the root
	/// This ensures every node is part of the scene graph
	if (!parent) {
		parent = this->root;
	}

	/// Add the node to its parent
	/// This also sets up the parent-child relationship
	parent->addChild(node);

	/// Flag for full update since hierarchy changed
	this->needsFullUpdate = true;

	spdlog::debug("Created node '{}' with parent '{}'", name, parent->getName());
	return node;
}

void Scene::removeNode(const std::shared_ptr<SceneNode>& node) {
	if (!node) {
		spdlog::warn("Attempted to remove null node from scene");
		return;
	}

	/// We don't allow removing the root or terrain root nodes
	/// This maintains scene graph integrity
	if (node == this->root || node == this->terrainRoot) {
		spdlog::warn("Attempted to remove {} node from scene",
			node == this->root ? "root" : "terrain root");
		return;
	}

	/// Count nodes to be removed (node + all descendants)
	std::function<size_t(const std::shared_ptr<SceneNode>&)> countNodes =
		[&countNodes](const std::shared_ptr<SceneNode>& n) -> size_t {
		size_t count = 1; /// Count this node
		for (const auto& child : n->getChildren()) {
			count += countNodes(child);
		}
		return count;
	};

	size_t nodesToRemove = countNodes(node);

	/// Remove node from its parent
	/// This breaks the parent-child relationship
	if (auto parent = node->getParent().lock()) {
		parent->removeChild(node);
	}

	/// Update node count and flag for full update
	this->nodeCount -= nodesToRemove;
	this->needsFullUpdate = true;

	spdlog::debug("Removed node '{}' and {} descendants from scene", node->getName(), nodesToRemove - 1);
}

void Scene::update(float deltaTime) {
	if (this->needsFullUpdate) {
		/// Perform full transform update from root when needed
		/// This ensures consistent state after structural changes
		this->updateTransforms(this->root, glm::mat4(1.0f));
		this->needsFullUpdate = false;
		spdlog::trace("Performed full scene update");
	} else {
		/// Only update nodes that are marked as dirty
		/// This optimization avoids unnecessary updates
		for (const auto& child : this->root->getChildren()) {
			this->updateTransforms(child, this->root->getWorldTransform());
		}
		spdlog::trace("Updated dirty nodes in scene");
	}
}

void Scene::updateTransforms(const std::shared_ptr<SceneNode>& node,
	const glm::mat4& parentTransform) {
	/// Update the node's world transform
	/// This cascades down through all children
	node->updateWorldTransform(parentTransform);
}

void Scene::getRenderData(const rendering::Camera& camera,
	std::vector<rendering::Mesh::RenderData>& outRenderData) const {
	/// Create frustum for culling
	/// We use the camera's view-projection matrix to build the frustum
	Frustum frustum = this->createFrustumFromCamera(camera);

	/// Clear any existing render data
	/// We want a fresh collection of visible objects
	outRenderData.clear();

	/// Collect render data from root node
	/// This recursively processes all visible nodes
	this->root->getRenderData(frustum, outRenderData);

	spdlog::trace("Collected render data for {} visible objects", outRenderData.size());
}

Frustum Scene::createFrustumFromCamera(const rendering::Camera& camera) const {
	/// Create view-projection matrix
	glm::mat4 projection = camera.getProjectionMatrix(
		16.0f / 9.0f); /// TODO: Get actual aspect ratio
	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 viewProj = projection * view;

	/// Create frustum for visibility testing
	return Frustum::createFromMatrix(viewProj);
}

} /// namespace lillugsi::scene