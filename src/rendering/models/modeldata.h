#pragma once

#include "rendering/vertex.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lillugsi::rendering {

/// Data structure for a single mesh primitive within a model
/// This represents one renderable piece of geometry with its own material
struct ModelMeshData {
	std::vector<Vertex> vertices;    /// Vertex data for this mesh
	std::vector<uint32_t> indices;   /// Index data defining triangles
	std::string materialName;        /// Name of the material to apply
	std::string name;                /// Name of this mesh for identification
};

/// A complete model with all its meshes and materials
/// This serves as an intermediate representation during model loading
/// before the final scene nodes and GPU resources are created
struct ModelData {
	std::string name;                      /// Name of the model
	std::vector<ModelMeshData> meshes;     /// All mesh primitives in the model
	bool hasAnimations{false};             /// Whether the model contains animations
	
	/// Material information extracted from the model file
	/// We store this separately from engine materials to decouple
	/// the file format from our internal representation
	struct MaterialInfo {
		std::string albedoTexturePath;      /// Path to base color texture
		std::string normalTexturePath;      /// Path to normal map texture
		std::string roughnessTexturePath;   /// Path to roughness texture
		std::string metallicTexturePath;    /// Path to metallic texture
		std::string occlusionTexturePath;   /// Path to ambient occlusion texture
		glm::vec4 baseColor{1.0f};          /// Base color and alpha
		float roughness{0.5f};              /// Roughness factor [0-1]
		float metallic{0.0f};               /// Metallic factor [0-1]
		float occlusionStrength{1.0f};      /// Occlusion factor [0-1]
		float normalScale{1.0f};            /// Normal map strength
		bool doubleSided{false};            /// Whether material should be rendered on both sides
	};
	
	/// Map of material names to their information
	std::unordered_map<std::string, MaterialInfo> materials;
	
	/// Node hierarchy information
	struct NodeInfo {
		std::string name;                      /// Name of this node
		glm::vec3 translation{0.0f};           /// Position of this node
		glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; /// Rotation of this node (quaternion)
		glm::vec3 scale{1.0f};                 /// Scale of this node
		int meshIndex{-1};                     /// Index into meshes array, or -1 if no mesh
		std::vector<int> children;             /// Indices of child nodes
	};
	
	/// All nodes in the model
	std::vector<NodeInfo> nodes;
	
	/// Index of the root node, or -1 if no hierarchy
	int rootNode{-1};
};

} /// namespace lillugsi::rendering