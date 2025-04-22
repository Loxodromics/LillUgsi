#include "meshextractor.h"
#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace lillugsi::rendering {

MeshExtractor::MeshExtractor(const tinygltf::Model& gltfModel)
	: gltfModel(gltfModel) {
}

ModelMeshData MeshExtractor::extractMeshData(
	int meshIndex,
	int primitiveIndex,
	bool calculateTangents) const {

	ModelMeshData meshData;

	/// Validate mesh index
	if (meshIndex < 0 || meshIndex >= static_cast<int>(this->gltfModel.meshes.size())) {
		spdlog::error("Invalid mesh index: {}", meshIndex);
		return meshData;
	}

	const auto& gltfMesh = this->gltfModel.meshes[meshIndex];

	/// Validate primitive index
	if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(gltfMesh.primitives.size())) {
		spdlog::error("Invalid primitive index {} for mesh {}", primitiveIndex, meshIndex);
		return meshData;
	}

	const auto& primitive = gltfMesh.primitives[primitiveIndex];

	/// Validate primitive topology
	/// Our engine currently only supports triangle lists
	if (!this->validatePrimitiveTopology(primitive)) {
		spdlog::error("Unsupported primitive topology for mesh {}:{}", meshIndex, primitiveIndex);
		return meshData;
	}

	/// Set mesh name
	meshData.name = gltfMesh.name.empty() ?
		"mesh_" + std::to_string(meshIndex) + "_" + std::to_string(primitiveIndex) :
		gltfMesh.name + "_" + std::to_string(primitiveIndex);

	/// Set material name
	/// glTF materials are referenced by index
	if (primitive.material >= 0 && primitive.material < static_cast<int>(this->gltfModel.materials.size())) {
		const auto& material = this->gltfModel.materials[primitive.material];
		if (!material.name.empty()) {
			meshData.materialName = material.name;
		} else {
			meshData.materialName = "material_" + std::to_string(primitive.material);
		}
	}

	/// First, determine the vertex count from the position attribute
	/// Position is a required attribute in glTF
	size_t vertexCount = 0;
	if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
		int posAccessorIndex = primitive.attributes.at("POSITION");
		if (posAccessorIndex >= 0 && posAccessorIndex < static_cast<int>(this->gltfModel.accessors.size())) {
			vertexCount = this->gltfModel.accessors[posAccessorIndex].count;
		}
	}

	if (vertexCount == 0) {
		spdlog::error("Mesh {}:{} has no position data", meshIndex, primitiveIndex);
		return meshData;
	}

	/// Reserve space for vertices
	meshData.vertices.resize(vertexCount);

	/// Extract vertex attributes
	this->extractPositions(meshData.vertices, primitive, vertexCount);
	this->extractNormals(meshData.vertices, primitive, vertexCount);
	this->extractTextureCoords(meshData.vertices, primitive, vertexCount);
	this->extractColors(meshData.vertices, primitive, vertexCount);

	/// Handle tangents - extract from model or calculate if needed
	bool hasTangents = this->extractTangents(meshData.vertices, primitive, vertexCount);

	/// Extract indices
	this->extractIndices(meshData.indices, primitive);

	/// Calculate tangents if not provided and requested
	if (!hasTangents && calculateTangents && !meshData.indices.empty()) {
		/// Tangent calculation requires indices for accurate results
		spdlog::debug("Calculating tangents for mesh {}:{}", meshIndex, primitiveIndex);
		TangentCalculator::calculateTangents(meshData.vertices, meshData.indices);
	}

	spdlog::debug("Extracted mesh {}:{} with {} vertices and {} indices",
		meshIndex, primitiveIndex, meshData.vertices.size(), meshData.indices.size());

	return meshData;
}

void MeshExtractor::extractPositions(
	std::vector<Vertex>& vertices,
	const tinygltf::Primitive& primitive,
	size_t vertexCount) const {

	/// Position attribute is required for valid meshes
	if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
		spdlog::error("Mesh is missing required POSITION attribute");
		return;
	}

	int accessorIndex = primitive.attributes.at("POSITION");
	auto [data, count] = this->getAccessorData(accessorIndex);

	if (!data || count != vertexCount) {
		spdlog::error("Invalid position data, expected {} vertices but got {}", vertexCount, count);
		return;
	}

	/// Get accessor info to determine component type
	const auto& accessor = this->gltfModel.accessors[accessorIndex];

	/// Positions should be 3D vectors
	if (accessor.type != TINYGLTF_TYPE_VEC3) {
		spdlog::error("Position attribute has wrong type, expected VEC3");
		return;
	}

	/// Handle different component types (unlikely for positions but handle anyway)
	if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
		/// Float positions (most common)
		const float* positions = reinterpret_cast<const float*>(data);
		for (size_t i = 0; i < count; ++i) {
			vertices[i].position = glm::vec3(
				positions[i * 3],     /// X
				positions[i * 3 + 1], /// Y
				positions[i * 3 + 2]  /// Z
			);
		}
	} else {
		/// Other types need conversion to float
		spdlog::warn("Position data in non-float format, conversion may lose precision");

		/// Calculate scale for normalized types
		float scale = 1.0f;
		if (accessor.normalized) {
			/// Scale depends on component type
			switch (accessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_BYTE:
					scale = 1.0f / 127.0f;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					scale = 1.0f / 255.0f;
					break;
				case TINYGLTF_COMPONENT_TYPE_SHORT:
					scale = 1.0f / 32767.0f;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					scale = 1.0f / 65535.0f;
					break;
			}
		}

		/// Handle different integer component types
		switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_BYTE: {
				const int8_t* positions = reinterpret_cast<const int8_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].position = glm::vec3(
						positions[i * 3] * scale,
						positions[i * 3 + 1] * scale,
						positions[i * 3 + 2] * scale
					);
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* positions = reinterpret_cast<const uint8_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].position = glm::vec3(
						positions[i * 3] * scale,
						positions[i * 3 + 1] * scale,
						positions[i * 3 + 2] * scale
					);
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_SHORT: {
				const int16_t* positions = reinterpret_cast<const int16_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].position = glm::vec3(
						positions[i * 3] * scale,
						positions[i * 3 + 1] * scale,
						positions[i * 3 + 2] * scale
					);
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* positions = reinterpret_cast<const uint16_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].position = glm::vec3(
						positions[i * 3] * scale,
						positions[i * 3 + 1] * scale,
						positions[i * 3 + 2] * scale
					);
				}
				break;
			}
			default:
				spdlog::error("Unsupported component type for positions: {}", accessor.componentType);
				break;
		}
	}
}

void MeshExtractor::extractNormals(
	std::vector<Vertex>& vertices,
	const tinygltf::Primitive& primitive,
	size_t vertexCount) const {

	/// Normals are optional, initialize to defaults if missing
	/// Default normals will be automatically calculated later if needed
	if (primitive.attributes.find("NORMAL") == primitive.attributes.end()) {
		spdlog::debug("Mesh has no normal data, using defaults");
		for (auto& vertex : vertices) {
			vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); /// Default up normal
		}
		return;
	}

	int accessorIndex = primitive.attributes.at("NORMAL");
	auto [data, count] = this->getAccessorData(accessorIndex);

	if (!data || count != vertexCount) {
		spdlog::warn("Invalid normal data, using defaults");
		for (auto& vertex : vertices) {
			vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
		}
		return;
	}

	/// Get accessor info
	const auto& accessor = this->gltfModel.accessors[accessorIndex];

	/// Normals should be 3D vectors
	if (accessor.type != TINYGLTF_TYPE_VEC3) {
		spdlog::error("Normal attribute has wrong type, expected VEC3");
		return;
	}

	/// Handle different component types
	if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
		/// Float normals (most common)
		const float* normals = reinterpret_cast<const float*>(data);
		for (size_t i = 0; i < count; ++i) {
			vertices[i].normal = glm::vec3(
				normals[i * 3],
				normals[i * 3 + 1],
				normals[i * 3 + 2]
			);

			/// Ensure normals are normalized
			/// Some models have non-normalized normals which can cause lighting issues
			if (glm::length(vertices[i].normal) > 0.0001f) {
				vertices[i].normal = glm::normalize(vertices[i].normal);
			} else {
				vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
			}
		}
	} else {
		/// Handle normalized integer formats
		/// These would be extremely rare for normals but handle them anyway
		spdlog::warn("Normal data in non-float format, conversion may affect quality");

		/// Calculate scale for normalized types
		float scale = 1.0f;
		if (accessor.normalized) {
			switch (accessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_BYTE:
					scale = 1.0f / 127.0f;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					scale = 1.0f / 255.0f;
					break;
				case TINYGLTF_COMPONENT_TYPE_SHORT:
					scale = 1.0f / 32767.0f;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					scale = 1.0f / 65535.0f;
					break;
			}
		}

		/// Process based on component type
		/// This handles the rare case of packed normals
		switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_BYTE: {
				const int8_t* normals = reinterpret_cast<const int8_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].normal = glm::normalize(glm::vec3(
						normals[i * 3] * scale,
						normals[i * 3 + 1] * scale,
						normals[i * 3 + 2] * scale
					));
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* normals = reinterpret_cast<const uint8_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].normal = glm::normalize(glm::vec3(
						normals[i * 3] * scale,
						normals[i * 3 + 1] * scale,
						normals[i * 3 + 2] * scale
					));
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_SHORT: {
				const int16_t* normals = reinterpret_cast<const int16_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].normal = glm::normalize(glm::vec3(
						normals[i * 3] * scale,
						normals[i * 3 + 1] * scale,
						normals[i * 3 + 2] * scale
					));
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* normals = reinterpret_cast<const uint16_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].normal = glm::normalize(glm::vec3(
						normals[i * 3] * scale,
						normals[i * 3 + 1] * scale,
						normals[i * 3 + 2] * scale
					));
				}
				break;
			}
			default:
				spdlog::error("Unsupported component type for normals: {}", accessor.componentType);
				break;
		}
	}
}

void MeshExtractor::extractTextureCoords(
	std::vector<Vertex>& vertices,
	const tinygltf::Primitive& primitive,
	size_t vertexCount) const {

	/// Texture coordinates are optional
	/// glTF supports multiple texture coordinate sets (TEXCOORD_0, TEXCOORD_1, etc.)
	/// We use TEXCOORD_0 as our default set
	if (primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end()) {
		spdlog::debug("Mesh has no texture coordinates, using defaults");
		for (auto& vertex : vertices) {
			/// Default to mapping the entire texture to the mesh
			vertex.texCoord = glm::vec2(0.0f);
		}
		return;
	}

	int accessorIndex = primitive.attributes.at("TEXCOORD_0");
	auto [data, count] = this->getAccessorData(accessorIndex);

	if (!data || count != vertexCount) {
		spdlog::warn("Invalid texture coordinate data, using defaults");
		for (auto& vertex : vertices) {
			vertex.texCoord = glm::vec2(0.0f);
		}
		return;
	}

	/// Get accessor info
	const auto& accessor = this->gltfModel.accessors[accessorIndex];

	/// Texture coordinates should be 2D vectors
	if (accessor.type != TINYGLTF_TYPE_VEC2) {
		spdlog::error("Texture coordinate attribute has wrong type, expected VEC2");
		return;
	}

	/// Calculate scale for normalized types
	float scale = 1.0f;
	if (accessor.normalized) {
		/// Scale depends on component type
		switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				scale = 1.0f / 127.0f;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				scale = 1.0f / 255.0f;
				break;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
				scale = 1.0f / 32767.0f;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				scale = 1.0f / 65535.0f;
				break;
		}
	}

	/// Process based on component type
	switch (accessor.componentType) {
		case TINYGLTF_COMPONENT_TYPE_FLOAT: {
			const float* texCoords = reinterpret_cast<const float*>(data);
			for (size_t i = 0; i < count; ++i) {
				vertices[i].texCoord = glm::vec2(
					texCoords[i * 2],     /// U
					texCoords[i * 2 + 1]  /// V
				);
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_BYTE: {
			const int8_t* texCoords = reinterpret_cast<const int8_t*>(data);
			for (size_t i = 0; i < count; ++i) {
				vertices[i].texCoord = glm::vec2(
					texCoords[i * 2] * scale,
					texCoords[i * 2 + 1] * scale
				);
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
			const uint8_t* texCoords = reinterpret_cast<const uint8_t*>(data);
			for (size_t i = 0; i < count; ++i) {
				vertices[i].texCoord = glm::vec2(
					texCoords[i * 2] * scale,
					texCoords[i * 2 + 1] * scale
				);
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_SHORT: {
			const int16_t* texCoords = reinterpret_cast<const int16_t*>(data);
			for (size_t i = 0; i < count; ++i) {
				vertices[i].texCoord = glm::vec2(
					texCoords[i * 2] * scale,
					texCoords[i * 2 + 1] * scale
				);
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
			const uint16_t* texCoords = reinterpret_cast<const uint16_t*>(data);
			for (size_t i = 0; i < count; ++i) {
				vertices[i].texCoord = glm::vec2(
					texCoords[i * 2] * scale,
					texCoords[i * 2 + 1] * scale
				);
			}
			break;
		}
		default:
			spdlog::error("Unsupported component type for texture coordinates: {}", accessor.componentType);
			break;
	}
}

void MeshExtractor::extractColors(
	std::vector<Vertex>& vertices,
	const tinygltf::Primitive& primitive,
	size_t vertexCount) const {

	/// Colors are optional
	if (primitive.attributes.find("COLOR_0") == primitive.attributes.end()) {
		/// If not provided, set default white color
		/// This ensures materials work correctly with vertex color inputs
		for (auto& vertex : vertices) {
			vertex.color = glm::vec3(1.0f);
		}
		return;
	}

	int accessorIndex = primitive.attributes.at("COLOR_0");
	auto [data, count] = this->getAccessorData(accessorIndex);

	if (!data || count != vertexCount) {
		spdlog::warn("Invalid color data, using defaults");
		for (auto& vertex : vertices) {
			vertex.color = glm::vec3(1.0f);
		}
		return;
	}

	/// Get accessor info
	const auto& accessor = this->gltfModel.accessors[accessorIndex];

	/// Calculate scale for normalized types
	float scale = 1.0f;
	if (accessor.normalized) {
		/// Scale depends on component type
		switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				scale = 1.0f / 127.0f;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				scale = 1.0f / 255.0f;
				break;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
				scale = 1.0f / 32767.0f;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				scale = 1.0f / 65535.0f;
				break;
		}
	}

	/// Extract colors based on type and component type
	/// glTF supports both RGB and RGBA color formats
	if (accessor.type == TINYGLTF_TYPE_VEC3) {
		/// RGB colors
		switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_FLOAT: {
				const float* colors = reinterpret_cast<const float*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].color = glm::vec3(
						colors[i * 3],     /// R
						colors[i * 3 + 1], /// G
						colors[i * 3 + 2]  /// B
					);
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* colors = reinterpret_cast<const uint8_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].color = glm::vec3(
						colors[i * 3] * scale,     /// R
						colors[i * 3 + 1] * scale, /// G
						colors[i * 3 + 2] * scale  /// B
					);
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* colors = reinterpret_cast<const uint16_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].color = glm::vec3(
						colors[i * 3] * scale,     /// R
						colors[i * 3 + 1] * scale, /// G
						colors[i * 3 + 2] * scale  /// B
					);
				}
				break;
			}
			default:
				spdlog::error("Unsupported component type for colors: {}", accessor.componentType);
				break;
		}
	} else if (accessor.type == TINYGLTF_TYPE_VEC4) {
		/// RGBA colors (we ignore alpha)
		switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_FLOAT: {
				const float* colors = reinterpret_cast<const float*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].color = glm::vec3(
						colors[i * 4],     /// R
						colors[i * 4 + 1], /// G
						colors[i * 4 + 2]  /// B
					);
					/// We ignore alpha (colors[i * 4 + 3])
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* colors = reinterpret_cast<const uint8_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].color = glm::vec3(
						colors[i * 4] * scale,     /// R
						colors[i * 4 + 1] * scale, /// G
						colors[i * 4 + 2] * scale  /// B
					);
					/// We ignore alpha (colors[i * 4 + 3])
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* colors = reinterpret_cast<const uint16_t*>(data);
				for (size_t i = 0; i < count; ++i) {
					vertices[i].color = glm::vec3(
						colors[i * 4] * scale,     /// R
						colors[i * 4 + 1] * scale, /// G
						colors[i * 4 + 2] * scale  /// B
					);
					/// We ignore alpha (colors[i * 4 + 3])
				}
				break;
			}
			default:
				spdlog::error("Unsupported component type for colors: {}", accessor.componentType);
				break;
		}
	} else {
		spdlog::error("Color attribute has wrong type, expected VEC3 or VEC4");

		/// Set default colors
		for (auto& vertex : vertices) {
			vertex.color = glm::vec3(1.0f);
		}
	}
}

bool MeshExtractor::extractTangents(
	std::vector<Vertex>& vertices,
	const tinygltf::Primitive& primitive,
	size_t vertexCount) const {

	/// Tangents are optional
	/// If not provided, they will be calculated later if needed
	if (primitive.attributes.find("TANGENT") == primitive.attributes.end()) {
		return false;
	}

	int accessorIndex = primitive.attributes.at("TANGENT");
	auto [data, count] = this->getAccessorData(accessorIndex);

	if (!data || count != vertexCount) {
		spdlog::warn("Invalid tangent data, will calculate later if needed");
		return false;
	}

	/// Get accessor info
	const auto& accessor = this->gltfModel.accessors[accessorIndex];

	/// Tangents should be 4D vectors (XYZ + handedness W)
	/// glTF defines tangents as vec4 where the w component represents handedness
	if (accessor.type != TINYGLTF_TYPE_VEC4) {
		spdlog::error("Tangent attribute has wrong type, expected VEC4");
		return false;
	}

	/// Tangents are almost always stored as floats
	if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
		const float* tangents = reinterpret_cast<const float*>(data);
		for (size_t i = 0; i < count; ++i) {
			vertices[i].tangent = glm::vec3(
				tangents[i * 4],     /// X
				tangents[i * 4 + 1], /// Y
				tangents[i * 4 + 2]  /// Z
			);
			/// We ignore the W component (handedness) as our engine doesn't use it
			/// If needed, we could store it in a separate attribute or compute it
		}
		return true;
	} else {
		spdlog::error("Unsupported component type for tangents: {}", accessor.componentType);
		return false;
	}
}

void MeshExtractor::extractIndices(
	std::vector<uint32_t>& indices,
	const tinygltf::Primitive& primitive) const {

	/// Indices are required for efficient rendering
	/// glTF allows non-indexed primitives, but we prefer indexed for performance
	if (primitive.indices < 0) {
		spdlog::warn("Mesh has no indices, generating simple sequential indices");

		/// Generate sequential indices (0, 1, 2, ...)
		/// This is inefficient but allows us to handle non-indexed geometry
		size_t vertexCount = 0;
		if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
			int accessorIndex = primitive.attributes.at("POSITION");
			if (accessorIndex >= 0 && accessorIndex < static_cast<int>(this->gltfModel.accessors.size())) {
				vertexCount = this->gltfModel.accessors[accessorIndex].count;
			}
		}

		/// For triangles, we need vertexCount indices
		indices.reserve(vertexCount);
		for (size_t i = 0; i < vertexCount; ++i) {
			indices.push_back(static_cast<uint32_t>(i));
		}
		return;
	}

	/// Get index data from the accessor
	auto [data, count] = this->getAccessorData(primitive.indices);

	if (!data || count == 0) {
		spdlog::error("Failed to get index data");
		return;
	}

	/// Get accessor information to determine index data type
	const auto& accessor = this->gltfModel.accessors[primitive.indices];

	/// Reserve space for indices
	indices.reserve(count);

	/// Convert indices to uint32_t regardless of source format
	/// glTF can use multiple index formats but our engine uses uint32_t
	switch (accessor.componentType) {
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
			/// 8-bit indices - these are rare but supported by glTF
			const uint8_t* indicesData = reinterpret_cast<const uint8_t*>(data);
			for (size_t i = 0; i < count; ++i) {
				indices.push_back(static_cast<uint32_t>(indicesData[i]));
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
			/// 16-bit indices - common for smaller meshes
			const uint16_t* indicesData = reinterpret_cast<const uint16_t*>(data);
			for (size_t i = 0; i < count; ++i) {
				indices.push_back(static_cast<uint32_t>(indicesData[i]));
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
			/// 32-bit indices - direct copy since our engine uses uint32_t
			const uint32_t* indicesData = reinterpret_cast<const uint32_t*>(data);
			for (size_t i = 0; i < count; ++i) {
				indices.push_back(indicesData[i]);
			}
			break;
		}
		default:
			spdlog::error("Unsupported index component type: {}", accessor.componentType);
			break;
	}
}

std::pair<const unsigned char*, size_t> MeshExtractor::getAccessorData(int accessorIndex) const {
	/// Validate accessor index
	if (accessorIndex < 0 || accessorIndex >= static_cast<int>(this->gltfModel.accessors.size())) {
		spdlog::error("Invalid accessor index: {}", accessorIndex);
		return {nullptr, 0};
	}

	const auto& accessor = this->gltfModel.accessors[accessorIndex];

	/// Get the buffer view referenced by the accessor
	if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(this->gltfModel.bufferViews.size())) {
		/// Some accessors might not have a buffer view (e.g., they could use default values)
		/// This is rare but valid in glTF
		spdlog::warn("Accessor {} has no valid buffer view", accessorIndex);
		return {nullptr, 0};
	}

	const auto& bufferView = this->gltfModel.bufferViews[accessor.bufferView];

	/// Get the buffer referenced by the buffer view
	if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(this->gltfModel.buffers.size())) {
		spdlog::error("Invalid buffer index in buffer view: {}", bufferView.buffer);
		return {nullptr, 0};
	}

	const auto& buffer = this->gltfModel.buffers[bufferView.buffer];

	/// Calculate the start of the data in the buffer
	/// We need to account for the buffer view offset and the accessor byte offset
	/// This is how glTF efficiently packs multiple data arrays into a single buffer
	const unsigned char* dataStart = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

	/// Return the pointer to the data and the count of elements
	return {dataStart, accessor.count};
}

int MeshExtractor::getComponentSize(int componentType) {
	/// Return size in bytes for each component type
	switch (componentType) {
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return 1;
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return 2;
		case TINYGLTF_COMPONENT_TYPE_INT:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return 4;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:
			return 8;
		default:
			spdlog::error("Unknown component type: {}", componentType);
			return 0;
	}
}

int MeshExtractor::getComponentCount(int type) {
	/// Return the number of components for each type
	switch (type) {
		case TINYGLTF_TYPE_SCALAR: return 1;
		case TINYGLTF_TYPE_VEC2:   return 2;
		case TINYGLTF_TYPE_VEC3:   return 3;
		case TINYGLTF_TYPE_VEC4:   return 4;
		case TINYGLTF_TYPE_MAT2:   return 4; /// 2x2 matrix
		case TINYGLTF_TYPE_MAT3:   return 9; /// 3x3 matrix
		case TINYGLTF_TYPE_MAT4:   return 16; /// 4x4 matrix
		default:
			spdlog::error("Unknown type: {}", type);
			return 0;
	}
}

bool MeshExtractor::validatePrimitiveTopology(const tinygltf::Primitive& primitive) const {
	/// Check if the primitive mode is supported by our renderer
	/// glTF supports various topology types (points, lines, triangles, etc.)
	/// Our engine currently only supports triangle lists
	switch (primitive.mode) {
		case TINYGLTF_MODE_TRIANGLES:
			/// Triangle list - fully supported
			return true;
		case TINYGLTF_MODE_TRIANGLE_STRIP:
		case TINYGLTF_MODE_TRIANGLE_FAN:
			/// Triangle strips and fans could be supported but need conversion
			spdlog::warn("Triangle strips and fans require conversion to triangle lists");
			return false;
		case TINYGLTF_MODE_POINTS:
		case TINYGLTF_MODE_LINE:
		case TINYGLTF_MODE_LINE_LOOP:
		case TINYGLTF_MODE_LINE_STRIP:
			/// Non-triangle primitives are not supported
			spdlog::error("Non-triangle primitive mode not supported: {}", primitive.mode);
			return false;
		default:
			/// Unknown mode
			spdlog::error("Unknown primitive mode: {}", primitive.mode);
			return false;
	}
}

} /// namespace lillugsi::rendering