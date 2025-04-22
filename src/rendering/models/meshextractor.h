#pragma once

#include "modeldata.h"
#include "rendering/tangentcalculator.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace tinygltf {
	class Model;
	class Primitive;
}

namespace lillugsi::rendering {

/// MeshExtractor handles the conversion of glTF mesh data to our engine's format
/// We use a dedicated class to encapsulate the complexity of mesh extraction
/// and keep the model loader focused on higher-level concerns
class MeshExtractor {
public:
	/// Create a mesh extractor
	/// @param gltfModel The glTF model containing mesh data
	explicit MeshExtractor(const tinygltf::Model& gltfModel);

	/// Extract mesh data from a glTF primitive
	/// @param meshIndex Index of the mesh in the glTF model
	/// @param primitiveIndex Index of the primitive in the mesh
	/// @param options Options for extraction (tangents, etc.)
	/// @return The extracted mesh data ready for rendering
	[[nodiscard]] ModelMeshData extractMeshData(
		int meshIndex,
		int primitiveIndex,
		bool calculateTangents) const;

private:
	/// Extract position attribute from glTF primitive
	/// @param vertices Vector of vertices to populate
	/// @param primitive The glTF primitive containing attributes
	/// @param vertexCount Expected number of vertices
	void extractPositions(
		std::vector<Vertex>& vertices,
		const tinygltf::Primitive& primitive,
		size_t vertexCount) const;

	/// Extract normal attribute from glTF primitive
	/// @param vertices Vector of vertices to populate
	/// @param primitive The glTF primitive containing attributes
	/// @param vertexCount Expected number of vertices
	void extractNormals(
		std::vector<Vertex>& vertices,
		const tinygltf::Primitive& primitive,
		size_t vertexCount) const;

	/// Extract texture coordinate attribute from glTF primitive
	/// @param vertices Vector of vertices to populate
	/// @param primitive The glTF primitive containing attributes
	/// @param vertexCount Expected number of vertices
	void extractTextureCoords(
		std::vector<Vertex>& vertices,
		const tinygltf::Primitive& primitive,
		size_t vertexCount) const;

	/// Extract color attribute from glTF primitive
	/// @param vertices Vector of vertices to populate
	/// @param primitive The glTF primitive containing attributes
	/// @param vertexCount Expected number of vertices
	void extractColors(
		std::vector<Vertex>& vertices,
		const tinygltf::Primitive& primitive,
		size_t vertexCount) const;

	/// Extract tangent attribute from glTF primitive
	/// @param vertices Vector of vertices to populate
	/// @param primitive The glTF primitive containing attributes
	/// @param vertexCount Expected number of vertices
	/// @return True if tangents were successfully extracted
	[[nodiscard]] bool extractTangents(
		std::vector<Vertex>& vertices,
		const tinygltf::Primitive& primitive,
		size_t vertexCount) const;

	/// Extract indices from glTF primitive
	/// @param indices Vector of indices to populate
	/// @param primitive The glTF primitive containing indices
	void extractIndices(
		std::vector<uint32_t>& indices,
		const tinygltf::Primitive& primitive) const;

	/// Get accessor data from glTF model
	/// @param accessorIndex Index of the accessor
	/// @return Pointer to data and count of elements
	[[nodiscard]] std::pair<const unsigned char*, size_t> getAccessorData(
		int accessorIndex) const;

	/// Get the component size in bytes for a glTF component type
	/// @param componentType The glTF component type
	/// @return Size in bytes (1, 2, 4, etc.)
	[[nodiscard]] static int getComponentSize(int componentType);

	/// Get the component count for a glTF type
	/// @param type The glTF type (SCALAR, VEC2, etc.)
	/// @return Number of components (1, 2, 3, 4)
	[[nodiscard]] static int getComponentCount(int type);

	/// Validate primitive topology for engine compatibility
	/// @param primitive The glTF primitive to validate
	/// @return True if the topology is supported
	[[nodiscard]] bool validatePrimitiveTopology(
		const tinygltf::Primitive& primitive) const;

	/// Reference to the glTF model being processed
	const tinygltf::Model& gltfModel;
};

} /// namespace lillugsi::rendering