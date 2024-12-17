#pragma once

#include "rendering/mesh.h"
#include "scene/boundingbox.h"
#include "scene/scenetypes.h"
#include "scene/frustum.h"
#include <memory>
#include <string>
#include <vector>

namespace lillugsi::scene {

/// SceneNode represents a node in the scene graph hierarchy
/// Each node can have a mesh, children, and transformations
/// The scene graph allows for hierarchical transformations and efficient culling
class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
	/// Create a scene node with the given name
	/// @param name The name of the node, used for identification and debugging
	explicit SceneNode(const std::string& name = "Node");

	/// We use virtual destructor since this class might be inherited from
	virtual ~SceneNode() = default;

	/// Prevent copying of scene nodes as they maintain unique resources
	/// and hierarchical relationships
	SceneNode(const SceneNode&) = delete;
	SceneNode& operator=(const SceneNode&) = delete;

	/// Add a child node to this node
	/// @param child The node to add as a child
	/// This establishes both parent-child relationships
	void addChild(std::shared_ptr<SceneNode> child);

	/// Remove a child node from this node
	/// @param child The node to remove
	/// This breaks both parent-child relationships
	void removeChild(const std::shared_ptr<SceneNode>& child);

	/// Set the mesh for this node
	/// @param mesh The mesh to associate with this node
	void setMesh(std::shared_ptr<rendering::Mesh> mesh);

	/// Set the local transform for this node
	/// @param transform The new local transform
	/// This triggers an update of world transforms for this node and its children
	void setLocalTransform(const Transform& transform);

	/// Get the world transform of this node
	/// @return The combined transform from root to this node
	const glm::mat4& getWorldTransform() const { return this->worldTransform; }

	/// Get the local transform of this node
	/// @return The local transform of this node
	const Transform& getLocalTransform() const { return this->localTransform; }

	/// Get the name of this node
	/// @return The node's name
	const std::string& getName() const { return this->name; }

	/// Get the parent of this node
	/// @return Weak pointer to parent node, or empty if this is the root
	std::weak_ptr<SceneNode> getParent() const { return this->parent; }

	/// Get all child nodes
	/// @return Vector of child node pointers
	const std::vector<std::shared_ptr<SceneNode>>& getChildren() const { return this->children; }

	/// Get the mesh associated with this node
	/// @return The node's mesh, or nullptr if none is set
	std::shared_ptr<rendering::Mesh> getMesh() const { return this->mesh; }

	/// Get the bounding box in world space
	/// @return The node's bounding box transformed to world space
	const BoundingBox& getWorldBounds() const { return this->worldBounds; }

	/// Update the world transform of this node and its children
	/// @param parentTransform The world transform of the parent node
	void updateWorldTransform(const glm::mat4& parentTransform = glm::mat4(1.0f));

	/// Check if this node is visible within the given frustum
	/// @param frustum The view frustum to test against
	/// @return true if the node's bounds intersect the frustum
	bool isVisible(const Frustum& frustum) const;

	/// Get render data for this node and visible children
	/// @param frustum The view frustum for visibility testing
	/// @param outRenderData Vector to store render data
	void getRenderData(const Frustum& frustum,
		std::vector<rendering::Mesh::RenderData>& outRenderData) const;

	/// Update bounds if they are marked as dirty
	/// @return true if bounds were updated
	void updateBoundsIfNeeded();

private:
	std::string name;                   /// Node identifier
	Transform localTransform;           /// Transform relative to parent
	glm::mat4 worldTransform;          /// Combined transform in world space
	std::weak_ptr<SceneNode> parent;   /// Parent node (weak to avoid cycles)
	std::vector<std::shared_ptr<SceneNode>> children;  /// Child nodes
	std::shared_ptr<rendering::Mesh> mesh;  /// Associated mesh
	BoundingBox localBounds;           /// Bounds in local space
	BoundingBox worldBounds;           /// Bounds in world space
	bool transformDirty;               /// Flag for transform updates

	/// Mark this node's transform as dirty
	/// This triggers updates in the next update cycle
	void markTransformDirty();

	/// Update the bounding box for this node
	/// This combines mesh bounds with child bounds
	void updateBounds();
	bool boundsDirty;                  /// Flag for bounds updates
};

} /// namespace lillugsi::scene