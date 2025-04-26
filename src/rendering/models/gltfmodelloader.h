#pragma once

#include "modelloader.h"
#include "modeldata.h"
#include "rendering/modelmesh.h"
#include "materialextractor.h"
#include "scenegraphconstructor.h"
#include "rendering/meshmanager.h"
#include "rendering/materialmanager.h"
#include "rendering/texturemanager.h"
#include "rendering/tangentcalculator.h"
#include <memory>
#include <string>
#include <vector>

/// Forward declarations to avoid including the entire tinygltf library
namespace tinygltf {
	class Model;
	class Node;
	class Primitive;
	class Material;
	class Texture;
	class Image;
}

namespace lillugsi::rendering {

/// GltfModelLoader provides loading and processing of glTF format models
/// We use the tinygltf library to parse glTF files and convert them to our
/// internal scene structure. This supports both .gltf (JSON) and .glb (binary) formats
class GltfModelLoader : public ModelLoader {
public:
	/// Create a glTF model loader
	/// @param meshManager Manager to create and manage meshes
	/// @param materialManager Manager to create and manage materials
	/// @param textureManager Manager to load and manage textures
	GltfModelLoader(
		std::shared_ptr<MeshManager> meshManager,
		std::shared_ptr<MaterialManager> materialManager,
		std::shared_ptr<TextureManager> textureManager);
	
	~GltfModelLoader() override = default;

	/// Load a glTF model from the given file into the scene
	/// @param filePath Path to the glTF (.gltf or .glb) file
	/// @param scene Scene to load the model into
	/// @param parentNode Parent node to attach the model to (optional)
	/// @param options Options controlling loading behavior
	/// @return Root node of the loaded model, or nullptr if loading failed
	[[nodiscard]] std::shared_ptr<scene::SceneNode> loadModel(
		const std::string& filePath,
		scene::Scene& scene,
		std::shared_ptr<scene::SceneNode> parentNode = nullptr,
		const ModelLoadOptions& options = ModelLoadOptions()) override;
		
	/// Check if this loader supports the given file format
	/// @param fileExtension The file extension to check (.gltf or .glb)
	/// @return True if this loader supports the format
	[[nodiscard]] bool supportsFormat(const std::string& fileExtension) const override;

private:
	/// Parse a glTF Model into our internal ModelData structure
	/// This extracts the node hierarchy, meshes, and materials
	/// @param gltfModel The parsed tinygltf model
	/// @param options Loading options
	/// @param baseDir Base directory for resolving relative paths
	/// @return Populated ModelData structure
	[[nodiscard]] ModelData parseGltfModel(
		const tinygltf::Model& gltfModel, 
		const ModelLoadOptions& options,
		const std::string& baseDir);
	
	/// Extract mesh data from a glTF mesh primitive
	/// This handles vertex attributes and indices
	/// @param gltfModel The parsed tinygltf model
	/// @param meshIndex Index of the mesh in the model
	/// @param primitiveIndex Index of the primitive in the mesh
	/// @param calculateTangents Whether to calculate tangent vectors
	/// @return Extracted mesh data ready for creating engine mesh
	[[nodiscard]] ModelMeshData extractMeshData(
		const tinygltf::Model& gltfModel,
		int meshIndex,
		int primitiveIndex,
		bool calculateTangents);
	
	/// Extract material properties from a glTF material
	/// This maps glTF PBR properties to our material system
	/// @param gltfModel The parsed tinygltf model
	/// @param materialIndex Index of the material in the model
	/// @param baseDir Base directory for resolving texture paths
	/// @return Material properties in our engine format
	[[nodiscard]] ModelData::MaterialInfo extractMaterialInfo(
		const tinygltf::Model& gltfModel,
		int materialIndex,
		const std::string& baseDir);
	
	/// Create PBR materials from extracted material info
	/// @param modelData Our internal model data with material info
	/// @param baseDir Base directory for resolving texture paths
	/// @return Map of material names to created material pointers
	[[nodiscard]] std::unordered_map<std::string, std::shared_ptr<PBRMaterial>> createMaterials(
		const ModelData& modelData,
		const std::string& baseDir);
	
	/// Create engine meshes from extracted mesh data
	/// @param modelData Our internal model data with mesh info
	/// @param materials Map of material names to created materials
	/// @return Vector of created meshes
	[[nodiscard]] std::vector<std::shared_ptr<Mesh>> createMeshes(
		const ModelData& modelData,
		const std::unordered_map<std::string, std::shared_ptr<PBRMaterial>>& materials);
	
	/// Get accessor data from a glTF buffer
	/// Helper to extract raw data from glTF buffer structures
	/// @param gltfModel The parsed tinygltf model
	/// @param accessorIndex Index of the accessor
	/// @return Pointer to the data and element count
	[[nodiscard]] std::pair<const unsigned char*, size_t> getAccessorData(
		const tinygltf::Model& gltfModel,
		int accessorIndex);
	
	/// Get texture path from glTF texture
	/// @param gltfModel The parsed tinygltf model
	/// @param textureIndex Index of the texture
	/// @param baseDir Base directory for resolving paths
	/// @return Full path to the texture file
	[[nodiscard]] std::string getTexturePath(
		const tinygltf::Model& gltfModel,
		int textureIndex,
		const std::string& baseDir);

	/// Normalizes a model's transform to make it properly fit in the scene viewport
	///
	/// Model files often use wildly different coordinate systems and scales.
	/// This method calculates the model's actual dimensions and adjusts its transform
	/// to ensure consistent sizing and positioning within our engine's coordinate space.
	/// We apply a single transform at the root node level to preserve the model's
	/// internal structure while making it properly viewable.
	///
	/// @param rootNode The root node of the model to normalize
	void normalizeModelTransform(const std::shared_ptr<scene::SceneNode>& rootNode);

	/// Recursively collects bounds from a node hierarchy for normalization
	///
	/// To properly normalize a model, we need accurate bounds information for the entire
	/// hierarchy. This method traverses the scene graph, accumulating bounds data while
	/// accounting for nested transformations. By collecting bounds this way rather than
	/// using the already-computed node bounds, we can handle models with improperly
	/// initialized bounds and ensure consistent scaling even for models with extreme
	/// coordinate values.
	///
	/// @param node The current node being processed
	/// @param bounds The accumulated bounds being built
	/// @param parentTransform The combined parent transform to apply to this node
	void collectNodeBounds(
		const std::shared_ptr<scene::SceneNode> &node,
		scene::BoundingBox& bounds,
		const glm::mat4& parentTransform);
	
	std::shared_ptr<MeshManager> meshManager;
	std::shared_ptr<MaterialManager> materialManager;
	std::shared_ptr<TextureManager> textureManager;
};

} /// namespace lillugsi::rendering