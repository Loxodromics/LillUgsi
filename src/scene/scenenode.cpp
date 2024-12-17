#include "scene/scenenode.h"
#include "scene/frustum.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace lillugsi::scene {

SceneNode::SceneNode(const std::string& name)
	: name(name)
	, localTransform()  /// Initialize with identity transform
	, worldTransform(1.0f)  /// Initialize with identity matrix
	, transformDirty(true)  /// Start with dirty transform to ensure initial update
	, boundsDirty(true) {   /// Start with dirty bounds to ensure initial update
	spdlog::debug("Created SceneNode '{}'", this->name);
}

void SceneNode::addChild(std::shared_ptr<SceneNode> child) {
	/// Validate input
	if (!child) {
		spdlog::warn("Attempted to add null child to SceneNode '{}'", this->name);
		return;
	}

	/// Check if child already has a parent
	if (const auto existingParent = child->parent.lock()) {
		if (existingParent.get() == this) {
			spdlog::warn("Attempted to add child '{}' to SceneNode '{}' multiple times",
				child->name, this->name);
			return;
		}
		/// Remove child from its current parent
		existingParent->removeChild(child);
	}

	/// Add the child and set up parent relationship
	this->children.push_back(child);
	child->parent = weak_from_this();

	/// Update child's world transform
	child->updateWorldTransform(this->worldTransform);

	/// Mark bounds as dirty since adding a child affects the combined bounds
	this->boundsDirty = true;

	/// Update the child's world transform
	child->updateWorldTransform(this->worldTransform);

	spdlog::debug("Added child '{}' to SceneNode '{}'", child->name, this->name);
}

void SceneNode::removeChild(const std::shared_ptr<SceneNode>& child) {
	/// Find and remove the child
	const auto it = std::find(this->children.begin(), this->children.end(), child);
	if (it != this->children.end()) {
		/// Clear the parent relationship
		(*it)->parent.reset();

		/// Remove from children vector
		this->children.erase(it);

		/// Update bounds hierarchy from this node up to root
		auto current = shared_from_this();
		while (current) {
			current->updateBounds();
			current = current->getParent().lock();
		}

		spdlog::debug("Removed child '{}' from SceneNode '{}'", child->name, this->name);
	}
}

void SceneNode::setMesh(std::shared_ptr<rendering::Mesh> mesh) {
	this->mesh = mesh;
	this->boundsDirty = true;  /// Mark bounds as dirty
	this->updateBounds();      /// Update bounds immediately
	spdlog::debug("Set mesh for SceneNode '{}'", this->name);
}

void SceneNode::setLocalTransform(const Transform& transform) {
	this->localTransform = transform;
	this->markTransformDirty();
	spdlog::trace("Set local transform for SceneNode '{}'", this->name);
}

void SceneNode::updateWorldTransform(const glm::mat4& parentTransform) {
	/// Calculate new world transform by combining parent transform with local transform
	this->worldTransform = parentTransform * this->localTransform.toMatrix();

	/// Update world bounds if transform has changed
	if (this->transformDirty) {
		this->worldBounds = this->localBounds.transform(this->worldTransform);
		this->transformDirty = false;
	}

	/// Recursively update all children
	for (const auto& child : this->children) {
		child->updateWorldTransform(this->worldTransform);
	}

	spdlog::trace("Updated world transform for SceneNode '{}' and children", this->name);
}

bool SceneNode::isVisible(const Frustum& frustum) const {
	/// If bounds are dirty, they need to be updated before visibility test
	/// This shouldn't happen in practice as bounds should be updated during scene update
	if (this->boundsDirty) {
		spdlog::warn("Testing visibility of SceneNode '{}' with dirty bounds", this->name);
	}

	/// Test if the node's world bounds intersect the frustum
	return frustum.intersectsBox(this->worldBounds);
}

void SceneNode::getRenderData(const Frustum& frustum,
	std::vector<rendering::Mesh::RenderData>& outRenderData) const {
	/// First check if this node is visible
	if (!this->isVisible(frustum)) {
		return;  /// Early out if not visible
	}

	/// If we have a mesh, add its render data
	if (this->mesh) {
		rendering::Mesh::RenderData data;
		this->mesh->prepareRenderData(data);
		/// Override the model matrix with our world transform
		data.modelMatrix = this->worldTransform;
		outRenderData.push_back(std::move(data));
	}

	/// Recursively collect render data from visible children
	for (const auto& child : this->children) {
		child->getRenderData(frustum, outRenderData);
	}
}

void SceneNode::updateBounds() {
	/// Start with an empty bounding box
	this->localBounds.reset();

	/// Add mesh bounds if we have a mesh
	if (this->mesh) {
		/// For now, we'll compute a simple bounds from vertices
		/// This could be optimized by caching bounds in the Mesh class
		const auto& vertices = this->mesh->getVertices();
		for (const auto& vertex : vertices) {
			this->localBounds.addPoint(vertex.position);
		}
	}

	/// Add transformed bounds of all children
	for (const auto& child : this->children) {
		/// Ensure child bounds are up to date
		if (child->boundsDirty) {
			child->updateBounds();
		}

		/// Transform child bounds to our local space and add them
		BoundingBox childLocalBounds = child->localBounds.transform(
			child->localTransform.toMatrix());

		/// Add all corners of the child bounds
		auto corners = childLocalBounds.getCorners();
		for (const auto& corner : corners) {
			this->localBounds.addPoint(corner);
		}
	}

	/// Update world bounds
	this->worldBounds = this->localBounds.transform(this->worldTransform);
	this->boundsDirty = false;

	spdlog::trace("Updated bounds for SceneNode '{}'", this->name);
}

void SceneNode::updateBoundsIfNeeded() {
	if (this->boundsDirty) {
		this->updateBounds();
	}
}

void SceneNode::markTransformDirty() {
	this->transformDirty = true;
	this->boundsDirty = true;  /// Transform changes affect world bounds

	/// Recursively mark all children as dirty
	/// Children's world transforms depend on our transform
	for (const auto& child : this->children) {
		child->markTransformDirty();
	}
}

} /// namespace lillugsi::scene