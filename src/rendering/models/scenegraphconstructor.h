#pragma once

#include "modeldata.h"
#include "modelloader.h"
#include "scene/scene.h"
#include "rendering/mesh.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace tinygltf {
	class Model;
	class Node;
}

namespace lillugsi::rendering {

/// SceneGraphConstructor builds a scene hierarchy from glTF model data
/// This class handles the conversion of glTF nodes to our engine's scene nodes
/// and ensures proper hierarchy, transformations, and mesh assignments
class SceneGraphConstructor {
public:
	/// Create a scene graph constructor
	/// @param gltfModel The glTF model containing node data
	/// @param modelData Our internal model data
	/// @param meshes Vector of prepared meshes from the model
	SceneGraphConstructor(
		const tinygltf::Model& gltfModel,
		const ModelData& modelData,
		const std::vector<std::shared_ptr<Mesh>>& meshes);
	
	/// Build scene graph from the default or first scene in the glTF file
	/// @param scene The scene to add nodes to
	/// @param parentNode Parent node to attach the root nodes to
	/// @param options Loading options like scaling
	/// @return Root node of the constructed scene graph
	[[nodiscard]] std::shared_ptr<scene::SceneNode> buildSceneGraph(
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode,
		const ModelLoadOptions& options);
	
private:
	/// Process a single node in the glTF scene hierarchy
	/// @param nodeIndex Index of the node to process
	/// @param scene The scene to add nodes to
	/// @param parentNode Parent node to attach this node to
	/// @param options Loading options like scaling
	/// @return The created scene node
	[[nodiscard]] std::shared_ptr<scene::SceneNode> processNode(
		int nodeIndex,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode,
		const ModelLoadOptions& options);
	
	/// Apply glTF node transformations to a scene node
	/// @param nodeInfo The node data from our model data
	/// @param sceneNode The scene node to apply transformations to
	/// @param scale Global scaling factor from options
	void applyNodeTransform(
		const ModelData::NodeInfo& nodeInfo,
		std::shared_ptr<scene::SceneNode> sceneNode,
		float scale);
	
	/// Assign a mesh to a scene node
	/// @param nodeInfo The node data from our model data
	/// @param sceneNode The scene node to assign the mesh to
	/// @return True if a mesh was assigned
	bool assignNodeMesh(
		const ModelData::NodeInfo& nodeInfo,
		std::shared_ptr<scene::SceneNode> sceneNode);
	
	/// Handle primitive groups for nodes with multiple mesh primitives
	/// @param meshIndex The index of the mesh in the glTF model
	/// @param nodeName The name of the current node
	/// @param scene The scene to add child nodes to
	/// @param parentNode Parent node to attach primitive nodes to
	/// @return True if child nodes were created for primitives
	bool handlePrimitiveGroups(
		int meshIndex,
		const std::string& nodeName,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode);
	
	/// Reference to the glTF model being processed
	const tinygltf::Model& gltfModel;
	
	/// Reference to our internal model data
	const ModelData& modelData;
	
	/// Reference to the vector of prepared meshes
	const std::vector<std::shared_ptr<Mesh>>& meshes;
	
	/// Map to track which node corresponds to which modelData node index
	/// This helps with scene construction and avoids duplicates
	std::unordered_map<int, std::shared_ptr<scene::SceneNode>> nodeMap;
};

} /// namespace lillugsi::rendering