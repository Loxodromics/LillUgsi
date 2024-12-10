#pragma once

#include "scenenode.h"
#include "frustum.h"
#include "rendering/camera.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace lillugsi::scene {

/// Scene class manages the complete scene graph and coordinates all scene operations
/// This class serves as the main interface for scene manipulation and rendering
class Scene {
public:
	/// Constructor creates an empty scene with a root node
	Scene();

	/// Prevent copying of scenes as they maintain unique resources
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	/// Create a new node in the scene with the given name
	/// @param name The name for the new node
	/// @param parent Parent node, or nullptr to add to root
	/// @return Shared pointer to the created node
	std::shared_ptr<SceneNode> createNode(
		const std::string& name = "Node",
		std::shared_ptr<SceneNode> parent = nullptr);

	/// Remove a node and all its children from the scene
	/// @param node The node to remove
	void removeNode(const std::shared_ptr<SceneNode>& node);

	/// Update the entire scene
	/// This updates transforms, bounds, and other scene state
	/// @param deltaTime Time elapsed since last update in seconds
	void update(float deltaTime);

	/// Get render data for all visible objects
	/// @param camera The camera to use for frustum culling
	/// @param outRenderData Vector to store render data for visible objects
	void getRenderData(const rendering::Camera& camera,
		std::vector<rendering::Mesh::RenderData>& outRenderData) const;

	/// Get the root node of the scene
	/// @return Shared pointer to the root node
	std::shared_ptr<SceneNode> getRoot() const { return this->root; }

	/// Get the total number of nodes in the scene
	/// @return The total node count
	size_t getNodeCount() const { return this->nodeCount; }

	/// Set the terrain root node
	/// This node will be used as the parent for terrain-specific nodes
	/// @param node The node to use as terrain root
	void setTerrainRoot(std::shared_ptr<SceneNode> node) { this->terrainRoot = node; }

	/// Get the terrain root node
	/// @return Shared pointer to the terrain root node
	std::shared_ptr<SceneNode> getTerrainRoot() const { return this->terrainRoot; }

private:
	std::shared_ptr<SceneNode> root;        /// Root node of the scene graph
	std::shared_ptr<SceneNode> terrainRoot; /// Special root for terrain nodes
	size_t nodeCount;                       /// Total number of nodes in scene

	/// Track if the scene needs a full update
	/// This is set when structural changes occur
	bool needsFullUpdate;

	/// Update transforms starting from a specific node
	/// @param node The node to start updating from
	/// @param parentTransform The world transform of the parent
	void updateTransforms(const std::shared_ptr<SceneNode>& node,
		const glm::mat4& parentTransform);

	/// Create a frustum from camera for culling
	/// @param camera The camera to create frustum from
	/// @return A frustum in world space
	Frustum createFrustumFromCamera(const rendering::Camera& camera) const;

	/// Initialize the scene with default nodes
	/// Called from constructor to set up initial scene structure
	void initialize();
};

} /// namespace lillugsi::scene