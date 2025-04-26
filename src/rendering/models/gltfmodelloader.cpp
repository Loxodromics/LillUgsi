#include "gltfmodelloader.h"
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

namespace lillugsi::rendering {

GltfModelLoader::GltfModelLoader(
	std::shared_ptr<MeshManager> meshManager,
	std::shared_ptr<MaterialManager> materialManager,
	std::shared_ptr<TextureManager> textureManager)
	: meshManager(std::move(meshManager))
	, materialManager(std::move(materialManager))
	, textureManager(std::move(textureManager)) {

	spdlog::info("glTF model loader created");
}

bool GltfModelLoader::supportsFormat(const std::string& fileExtension) const {
	/// Convert to lowercase for case-insensitive comparison
	/// This ensures we match extensions regardless of capitalization
	std::string ext = fileExtension;
	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](unsigned char c) { return std::tolower(c); });

	/// We support both text and binary glTF formats
	return ext == ".gltf" || ext == ".glb";
}

std::shared_ptr<scene::SceneNode> GltfModelLoader::loadModel(
	const std::string& filePath,
	scene::Scene& scene,
	std::shared_ptr<scene::SceneNode> parentNode,
	const ModelLoadOptions& options) {
	
	/// Ensure we have a valid parent node, defaulting to scene root if none provided
	if (!parentNode) {
		parentNode = scene.getRoot();
	}
	
	/// Extract base name from path for node naming
	std::filesystem::path path(filePath);
	std::string baseName = path.stem().string();
	std::string baseDir = path.parent_path().string();
	
	/// Create a root node for the model
	auto modelRootNode = scene.createNode(baseName, parentNode);
	
	/// Parse the glTF file using tinygltf
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF loader;
	std::string err, warn;
	
	bool success = false;
	
	/// Load the appropriate format based on file extension
	if (path.extension() == ".glb") {
		success = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath);
	} else {
		success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);
	}
	
	/// Log any warnings - these aren't fatal but might indicate issues
	if (!warn.empty()) {
		spdlog::warn("glTF warning while loading '{}': {}", filePath, warn);
	}
	
	/// Check if loading was successful
	if (!success) {
		spdlog::error("Failed to load glTF model '{}': {}", filePath, err);
		/// Remove the model root node since loading failed
		scene.removeNode(modelRootNode);
		return nullptr;
	}
	
	/// Parse the glTF model into our intermediate representation
	spdlog::debug("Parsing glTF model '{}'", filePath);
	ModelData modelData = this->parseGltfModel(gltfModel, options, baseDir);
	
	/// Create materials from the model data
	auto materials = this->createMaterials(modelData, baseDir);
	
	/// Create meshes from the model data
	auto meshes = this->createMeshes(modelData, materials);
	
	/// Build the scene hierarchy using our dedicated constructor
	SceneGraphConstructor sceneConstructor(gltfModel, modelData, meshes);
	auto rootNode = sceneConstructor.buildSceneGraph(scene, modelRootNode, options);

	normalizeModelTransform(modelRootNode);
	
	/// Update the model bounds to ensure proper culling
	modelRootNode->updateBoundsIfNeeded();
	
	spdlog::info("Successfully loaded glTF model '{}'", filePath);
	return modelRootNode;
}

ModelData GltfModelLoader::parseGltfModel(
	const tinygltf::Model& gltfModel,
	const ModelLoadOptions& options,
	const std::string& baseDir) {

	ModelData modelData;

	/// Extract the model's default name if available
	/// Otherwise use a generic name
	/*if (!gltfModel.asset.name.empty()) {
		modelData.name = gltfModel.asset.name;
	} else*/ {
		modelData.name = "GltfModel";
	}

	/// Parse materials using our dedicated extractor
	/// This encapsulates all the complex material extraction logic
	MaterialExtractor materialExtractor(gltfModel);
	modelData.materials = materialExtractor.extractAllMaterials(baseDir);

	/// Parse meshes
	/// We extract all mesh data from the glTF model
	spdlog::debug("Parsing {} meshes", gltfModel.meshes.size());
	for (size_t meshIndex = 0; meshIndex < gltfModel.meshes.size(); ++meshIndex) {
		const auto& gltfMesh = gltfModel.meshes[meshIndex];

		/// A single glTF mesh can contain multiple primitives (submeshes)
		/// Each primitive gets its own ModelMeshData entry
		for (size_t primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex) {
			/// Extract the mesh data from this primitive
			ModelMeshData meshData = this->extractMeshData(
				gltfModel,
				static_cast<int>(meshIndex),
				static_cast<int>(primitiveIndex),
				options.calculateTangents);

			/// Generate a name for the mesh if not already set during extraction
			if (meshData.name.empty()) {
				meshData.name = gltfMesh.name.empty() ?
					"mesh_" + std::to_string(meshIndex) + "_" + std::to_string(primitiveIndex) :
					gltfMesh.name + "_" + std::to_string(primitiveIndex);
			}

			/// Add the mesh data to our model data
			modelData.meshes.push_back(std::move(meshData));
		}
	}

	/// Parse node hierarchy
	/// We build a representation of the scene graph structure
	spdlog::debug("Parsing {} nodes", gltfModel.nodes.size());
	modelData.nodes.resize(gltfModel.nodes.size());

	for (size_t i = 0; i < gltfModel.nodes.size(); ++i) {
		const auto& gltfNode = gltfModel.nodes[i];
		auto& node = modelData.nodes[i];

		/// Set node name
		node.name = gltfNode.name.empty() ? "node_" + std::to_string(i) : gltfNode.name;

		/// Store mesh index if this node has a mesh
		node.meshIndex = gltfNode.mesh;

		/// Parse node transform
		/// glTF allows transforms to be specified in different ways:
		/// 1. Matrix: A 4x4 transform matrix
		/// 2. TRS: Separate translation, rotation, and scale
		if (!gltfNode.matrix.empty()) {
			/// Matrix form - decompose into TRS
			glm::mat4 matrix = glm::make_mat4(gltfNode.matrix.data());

			/// Decompose the matrix to extract translation, rotation, and scale
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(matrix, node.scale, node.rotation, node.translation, skew, perspective);
		} else {
			/// TRS form - direct assignment

			/// Translation
			if (!gltfNode.translation.empty()) {
				node.translation = glm::vec3(
					gltfNode.translation[0],
					gltfNode.translation[1],
					gltfNode.translation[2]);
			}

			/// Rotation (stored as quaternion)
			if (!gltfNode.rotation.empty()) {
				node.rotation = glm::quat(
					gltfNode.rotation[3], /// w comes last in glTF but first in glm
					gltfNode.rotation[0],
					gltfNode.rotation[1],
					gltfNode.rotation[2]);
			}

			/// Scale
			if (!gltfNode.scale.empty()) {
				node.scale = glm::vec3(
					gltfNode.scale[0],
					gltfNode.scale[1],
					gltfNode.scale[2]);
			}
		}

		/// Apply global scale from options
		node.scale *= options.scale;

		/// Store child indices
		node.children = gltfNode.children;
	}

	/// Determine if the model has animations
	modelData.hasAnimations = !gltfModel.animations.empty();
	if (modelData.hasAnimations) {
		spdlog::info("Model contains {} animations", gltfModel.animations.size());
	}

	spdlog::debug("Parsed glTF model with {} meshes, {} materials, and {} nodes",
		modelData.meshes.size(), modelData.materials.size(), modelData.nodes.size());

	return modelData;
}

ModelMeshData GltfModelLoader::extractMeshData(
	const tinygltf::Model& gltfModel,
	int meshIndex,
	int primitiveIndex,
	bool calculateTangents) {

	ModelMeshData meshData;

	/// Get references to the mesh and primitive
	const auto& gltfMesh = gltfModel.meshes[meshIndex];

	/// Validate primitive index
	if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(gltfMesh.primitives.size())) {
		spdlog::error("Invalid primitive index {} for mesh {}", primitiveIndex, meshIndex);
		return meshData;
	}

	const auto& primitive = gltfMesh.primitives[primitiveIndex];

	/// Set mesh name
	meshData.name = gltfMesh.name.empty() ?
		"mesh_" + std::to_string(meshIndex) + "_" + std::to_string(primitiveIndex) :
		gltfMesh.name + "_" + std::to_string(primitiveIndex);

	/// Set material name
	/// glTF materials are referenced by index
	if (primitive.material >= 0 && primitive.material < static_cast<int>(gltfModel.materials.size())) {
		const auto& material = gltfModel.materials[primitive.material];
		if (!material.name.empty()) {
			meshData.materialName = material.name;
		} else {
			meshData.materialName = "material_" + std::to_string(primitive.material);
		}
	}

	/// Extract indices
	/// glTF stores indices in an accessor
	if (primitive.indices >= 0) {
		auto [data, count] = this->getAccessorData(gltfModel, primitive.indices);
		if (data && count > 0) {
			/// Get accessor information to determine index data type
			const auto& accessor = gltfModel.accessors[primitive.indices];

			/// Reserve space for indices
			meshData.indices.reserve(count);

			/// Convert indices to uint32_t regardless of source format
			/// glTF can use multiple index formats but our engine uses uint32_t
			switch (accessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
					/// 8-bit indices
					const uint8_t* indices = reinterpret_cast<const uint8_t*>(data);
					for (size_t i = 0; i < count; ++i) {
						meshData.indices.push_back(static_cast<uint32_t>(indices[i]));
					}
					break;
				}
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
					/// 16-bit indices
					const uint16_t* indices = reinterpret_cast<const uint16_t*>(data);
					for (size_t i = 0; i < count; ++i) {
						meshData.indices.push_back(static_cast<uint32_t>(indices[i]));
					}
					break;
				}
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
					/// 32-bit indices - direct copy
					const uint32_t* indices = reinterpret_cast<const uint32_t*>(data);
					for (size_t i = 0; i < count; ++i) {
						meshData.indices.push_back(indices[i]);
					}
					break;
				}
				default:
					spdlog::error("Unsupported index component type: {}", accessor.componentType);
					break;
			}
		}
	}

	/// Extract vertex attributes
	/// First, determine the vertex count from the position attribute
	size_t vertexCount = 0;
	if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
		int posAccessorIndex = primitive.attributes.at("POSITION");
		if (posAccessorIndex >= 0 && posAccessorIndex < static_cast<int>(gltfModel.accessors.size())) {
			vertexCount = gltfModel.accessors[posAccessorIndex].count;
		}
	}

	/// Reserve space for vertices
	meshData.vertices.resize(vertexCount);

	/// glTF stores vertex attributes in separate accessors
	/// We need to extract each attribute separately and combine them

	/// Extract positions
	if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
		int accessorIndex = primitive.attributes.at("POSITION");
		auto [data, count] = this->getAccessorData(gltfModel, accessorIndex);

		if (data && count == vertexCount) {
			const float* positions = reinterpret_cast<const float*>(data);
			for (size_t i = 0; i < count; ++i) {
				meshData.vertices[i].position = glm::vec3(
					positions[i * 3],     /// X
					positions[i * 3 + 1], /// Y
					positions[i * 3 + 2]  /// Z
				);
			}
		}
	}

	/// Extract normals
	if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
		int accessorIndex = primitive.attributes.at("NORMAL");
		auto [data, count] = this->getAccessorData(gltfModel, accessorIndex);

		if (data && count == vertexCount) {
			const float* normals = reinterpret_cast<const float*>(data);
			for (size_t i = 0; i < count; ++i) {
				meshData.vertices[i].normal = glm::vec3(
					normals[i * 3],     /// X
					normals[i * 3 + 1], /// Y
					normals[i * 3 + 2]  /// Z
				);
			}
		}
	}

	/// Extract texture coordinates
	if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
		int accessorIndex = primitive.attributes.at("TEXCOORD_0");
		auto [data, count] = this->getAccessorData(gltfModel, accessorIndex);

		if (data && count == vertexCount) {
			const float* texCoords = reinterpret_cast<const float*>(data);
			for (size_t i = 0; i < count; ++i) {
				meshData.vertices[i].texCoord = glm::vec2(
					texCoords[i * 2],     /// U
					texCoords[i * 2 + 1]  /// V
				);
			}
		}
	}

	/// Extract colors
	if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
		int accessorIndex = primitive.attributes.at("COLOR_0");
		auto [data, count] = this->getAccessorData(gltfModel, accessorIndex);

		if (data && count == vertexCount) {
			const auto& accessor = gltfModel.accessors[accessorIndex];

			/// Color data can be encoded in different ways in glTF
			if (accessor.type == TINYGLTF_TYPE_VEC3) {
				/// RGB colors
				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
					/// Float RGB
					const float* colors = reinterpret_cast<const float*>(data);
					for (size_t i = 0; i < count; ++i) {
						meshData.vertices[i].color = glm::vec3(
							colors[i * 3],     /// R
							colors[i * 3 + 1], /// G
							colors[i * 3 + 2]  /// B
						);
					}
				} else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
					/// 8-bit RGB (normalize to 0-1 range)
					const uint8_t* colors = reinterpret_cast<const uint8_t*>(data);
					for (size_t i = 0; i < count; ++i) {
						meshData.vertices[i].color = glm::vec3(
							colors[i * 3] / 255.0f,     /// R
							colors[i * 3 + 1] / 255.0f, /// G
							colors[i * 3 + 2] / 255.0f  /// B
						);
					}
				}
			} else if (accessor.type == TINYGLTF_TYPE_VEC4) {
				/// RGBA colors (we ignore alpha)
				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
					/// Float RGBA
					const float* colors = reinterpret_cast<const float*>(data);
					for (size_t i = 0; i < count; ++i) {
						meshData.vertices[i].color = glm::vec3(
							colors[i * 4],     /// R
							colors[i * 4 + 1], /// G
							colors[i * 4 + 2]  /// B
						);
					}
				} else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
					/// 8-bit RGBA (normalize to 0-1 range)
					const uint8_t* colors = reinterpret_cast<const uint8_t*>(data);
					for (size_t i = 0; i < count; ++i) {
						meshData.vertices[i].color = glm::vec3(
							colors[i * 4] / 255.0f,     /// R
							colors[i * 4 + 1] / 255.0f, /// G
							colors[i * 4 + 2] / 255.0f  /// B
						);
					}
				}
			}
		}
	} else {
		/// If no vertex colors are provided, set default white
		/// This ensures materials work correctly with vertex color inputs
		for (auto& vertex : meshData.vertices) {
			vertex.color = glm::vec3(1.0f);
		}
	}

	/// Extract tangents if available
	if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
		int accessorIndex = primitive.attributes.at("TANGENT");
		auto [data, count] = this->getAccessorData(gltfModel, accessorIndex);

		if (data && count == vertexCount) {
			const float* tangents = reinterpret_cast<const float*>(data);
			for (size_t i = 0; i < count; ++i) {
				meshData.vertices[i].tangent = glm::vec3(
					tangents[i * 4],     /// X
					tangents[i * 4 + 1], /// Y
					tangents[i * 4 + 2]  /// Z
				);
				/// Note: we ignore tangent.w which is the handedness
				/// Our engine doesn't currently use this information
			}
		}
	} else if (calculateTangents) {
		/// Calculate tangents if not provided and requested
		/// This ensures normal mapping works correctly without requiring
		/// tangents to be stored in the model
		TangentCalculator::calculateTangents(meshData.vertices, meshData.indices);
	}

	spdlog::debug("Extracted mesh data for primitive {}:{} with {} vertices and {} indices",
		meshIndex, primitiveIndex, meshData.vertices.size(), meshData.indices.size());

	return meshData;
}

ModelData::MaterialInfo GltfModelLoader::extractMaterialInfo(
	const tinygltf::Model& gltfModel,
	int materialIndex,
	const std::string& baseDir) {
	
	/// Use our dedicated MaterialExtractor
	MaterialExtractor extractor(gltfModel);
	return extractor.extractMaterialInfo(materialIndex, baseDir);
}

std::unordered_map<std::string, std::shared_ptr<PBRMaterial>> GltfModelLoader::createMaterials(
	const ModelData& modelData,
	const std::string& baseDir) {
	
	std::unordered_map<std::string, std::shared_ptr<PBRMaterial>> materials;
	
	for (const auto& [name, materialInfo] : modelData.materials) {
		/// Create a PBR material using our material manager
		auto material = this->materialManager->createPBRMaterial(name);
		
		/// Set base material properties
		material->setBaseColor(materialInfo.baseColor);
		material->setMetallic(materialInfo.metallic);
		material->setRoughness(materialInfo.roughness);
		
		/// Load and assign textures
		
		/// Load albedo (base color) texture
		if (!materialInfo.albedoTexturePath.empty()) {
			auto texture = this->textureManager->getOrLoadTexture(
				materialInfo.albedoTexturePath,
				true, /// Generate mipmaps
				materialInfo.transparent ? 
					TextureLoader::Format::RGBA : /// Need alpha channel for transparency
					TextureLoader::Format::RGB     /// Save memory without alpha
			);
			
			if (texture) {
				material->setAlbedoTexture(texture);
				spdlog::debug("Set albedo texture for material {}: {}", 
					name, materialInfo.albedoTexturePath);
			}
		}
		
		/// Load normal map texture
		if (!materialInfo.normalTexturePath.empty()) {
			auto texture = this->textureManager->getOrLoadTexture(
				materialInfo.normalTexturePath,
				true, /// Generate mipmaps
				TextureLoader::Format::NormalMap /// Special format for normal maps
			);
			
			if (texture) {
				material->setNormalMap(texture, materialInfo.normalScale);
				spdlog::debug("Set normal map for material {}: {} (scale: {})", 
					name, materialInfo.normalTexturePath, materialInfo.normalScale);
			}
		}
		
		/// Check if we have a combined metallic-roughness texture
		if (!materialInfo.metallicTexturePath.empty() && 
			materialInfo.metallicTexturePath == materialInfo.roughnessTexturePath) {
			
			auto texture = this->textureManager->getOrLoadTexture(
				materialInfo.metallicTexturePath,
				true, /// Generate mipmaps
				TextureLoader::Format::RGBA /// Need all channels
			);
			
			if (texture) {
				/// Set the combined texture with channel mappings
				/// G channel contains roughness, B channel contains metallic
				material->setRoughnessMetallicMap(
					texture,
					Material::TextureChannel::G, /// Roughness channel
					Material::TextureChannel::B, /// Metallic channel
					materialInfo.roughness,
					materialInfo.metallic
				);
				
				spdlog::debug("Set combined roughness-metallic map for material {}: {}", 
					name, materialInfo.metallicTexturePath);
			}
		} else {
			/// Handle separate roughness and metallic textures
			
			/// Load roughness texture
			if (!materialInfo.roughnessTexturePath.empty()) {
				auto texture = this->textureManager->getOrLoadTexture(
					materialInfo.roughnessTexturePath,
					true, /// Generate mipmaps
					TextureLoader::Format::R /// Single channel is enough
				);
				
				if (texture) {
					material->setRoughnessMap(texture, materialInfo.roughness);
					spdlog::debug("Set roughness map for material {}: {}", 
						name, materialInfo.roughnessTexturePath);
				}
			}
			
			/// Load metallic texture
			if (!materialInfo.metallicTexturePath.empty()) {
				auto texture = this->textureManager->getOrLoadTexture(
					materialInfo.metallicTexturePath,
					true, /// Generate mipmaps
					TextureLoader::Format::R /// Single channel is enough
				);
				
				if (texture) {
					material->setMetallicMap(texture, materialInfo.metallic);
					spdlog::debug("Set metallic map for material {}: {}", 
						name, materialInfo.metallicTexturePath);
				}
			}
		}
		
		/// Load occlusion texture
		if (!materialInfo.occlusionTexturePath.empty()) {
			auto texture = this->textureManager->getOrLoadTexture(
				materialInfo.occlusionTexturePath,
				true, /// Generate mipmaps
				TextureLoader::Format::R /// Single channel is enough
			);
			
			if (texture) {
				material->setOcclusionMap(texture, materialInfo.occlusion);
				spdlog::debug("Set occlusion map for material {}: {}", 
					name, materialInfo.occlusionTexturePath);
			}
		}
		
		/// Load emissive texture if supported
		if (!materialInfo.emissiveTexturePath.empty()) {
			/// Note: Our current PBRMaterial may not support emissive textures
			/// This would be a good enhancement to add in the future
			spdlog::debug("Emissive textures not yet supported in material system: {}", 
				materialInfo.emissiveTexturePath);
		}
		
		/// Handle transparency if needed
		if (materialInfo.transparent) {
			/// Note: Our current implementation doesn't directly support
			/// setting the transparency mode from outside the material constructor.
			/// This would be an improvement to add to the Material class.
			spdlog::debug("Transparency for material {} not fully supported: mode={}, cutoff={}", 
				name, 
				static_cast<int>(materialInfo.alphaMode), 
				materialInfo.alphaCutoff);
		}
		
		/// Store the created material
		materials[name] = material;
	}
	
	spdlog::info("Created {} materials from model data", materials.size());
	return materials;
}

std::vector<std::shared_ptr<Mesh>> GltfModelLoader::createMeshes(
	const ModelData& modelData,
	const std::unordered_map<std::string, std::shared_ptr<PBRMaterial>>& materials) {

	std::vector<std::shared_ptr<Mesh>> meshes;
	meshes.reserve(modelData.meshes.size());

	/// Process each mesh in the model data
	for (const auto& meshData : modelData.meshes) {
		/// Skip meshes with no geometry
		if (meshData.vertices.empty()) {
			spdlog::warn("Skipping mesh '{}' with no vertices", meshData.name);
			continue;
		}

		/// Generate sequential indices if none exist
		std::vector<uint32_t> indices = meshData.indices;
		if (indices.empty()) {
			indices.reserve(meshData.vertices.size());
			for (size_t i = 0; i < meshData.vertices.size(); i++) {
				indices.push_back(static_cast<uint32_t>(i));
			}
			spdlog::debug("Generated {} sequential indices for non-indexed mesh '{}'",
				indices.size(), meshData.name);
		}

		/// Create a mesh with the extracted geometry
		auto mesh = this->meshManager->createMeshWithGeometry<ModelMesh>(
			meshData.vertices, indices);

		/// Assign material
		if (!meshData.materialName.empty() && materials.find(meshData.materialName) != materials.end()) {
			mesh->setMaterial(materials.at(meshData.materialName));
		} else {
			/// Assign default material if none specified or not found
			mesh->setMaterial(this->materialManager->getMaterial("default"));
		}

		/// Add to mesh list
		meshes.push_back(mesh);
	}

	spdlog::info("Created {} meshes from model data", meshes.size());
	return meshes;
}

std::pair<const unsigned char*, size_t> GltfModelLoader::getAccessorData(
	const tinygltf::Model& gltfModel,
	int accessorIndex) {

	/// Validate accessor index
	if (accessorIndex < 0 || accessorIndex >= static_cast<int>(gltfModel.accessors.size())) {
		spdlog::error("Invalid accessor index: {}", accessorIndex);
		return {nullptr, 0};
	}

	const auto& accessor = gltfModel.accessors[accessorIndex];

	/// Get the buffer view referenced by the accessor
	if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(gltfModel.bufferViews.size())) {
		/// Some accessors might not have a buffer view (e.g., they could use default values)
		/// This is rare but valid in glTF
		spdlog::warn("Accessor {} has no valid buffer view", accessorIndex);
		return {nullptr, 0};
	}

	const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];

	/// Get the buffer referenced by the buffer view
	if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(gltfModel.buffers.size())) {
		spdlog::error("Invalid buffer index in buffer view: {}", bufferView.buffer);
		return {nullptr, 0};
	}

	const auto& buffer = gltfModel.buffers[bufferView.buffer];

	/// Calculate the start of the data in the buffer
	/// We need to account for the buffer view offset and the accessor byte offset
	const unsigned char* dataStart = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

	/// Return the pointer to the data and the count of elements
	return {dataStart, accessor.count};
}

std::string GltfModelLoader::getTexturePath(
	const tinygltf::Model &gltfModel, int textureIndex, const std::string &baseDir) {
	/// Validate texture index
	if (textureIndex < 0 || textureIndex >= static_cast<int>(gltfModel.textures.size())) {
		spdlog::error("Invalid texture index: {}", textureIndex);
		return "";
	}

	const auto& texture = gltfModel.textures[textureIndex];

	/// Validate image index
	if (texture.source < 0 || texture.source >= static_cast<int>(gltfModel.images.size())) {
		spdlog::error("Invalid image index in texture: {}", texture.source);
		return "";
	}

	const auto& image = gltfModel.images[texture.source];

	/// Check if the image is embedded (data URI) or external (file path)
	if (!image.uri.empty()) {
		/// If the image is an external file, resolve the path
		/// We need to handle relative paths correctly

		/// Check if the URI is a data URI
		if (image.uri.find("data:") == 0) {
			/// Data URIs are embedded in the glTF file
			/// For our loader, we don't support direct loading from data URIs yet
			/// We would need to extract and save the data to a temporary file
			spdlog::warn("Data URI textures not fully supported: {}", image.uri.substr(0, 30) + "...");
			return "";
		}

		/// Construct the full path by combining base directory and relative path
		std::filesystem::path imagePath = baseDir.empty()
											  ? std::filesystem::path(image.uri)
											  : std::filesystem::path(baseDir) / image.uri;

		/// Return the normalized path
		return imagePath.lexically_normal().string();
	} else if (image.bufferView >= 0) {
		/// Image data is stored in the glTF buffer
		/// For our loader, we don't support direct loading from buffer data yet
		/// We would need to extract and save the data to a temporary file
		spdlog::warn("Embedded buffer textures not fully supported yet");
		return "";
	}

	/// If we reach here, the image data couldn't be found
	spdlog::error("Texture data not found for texture index: {}", textureIndex);
	return "";
}

void GltfModelLoader::normalizeModelTransform(const std::shared_ptr<scene::SceneNode>& rootNode) {
	/// Calculate bounds of the model
	scene::BoundingBox modelBounds;
	collectNodeBounds(rootNode, modelBounds, glm::mat4(1.0f));

	if (!modelBounds.isValid()) {
		spdlog::warn("Unable to normalize model with invalid bounds");
		return;
	}

	/// Get model size and center
	glm::vec3 modelSize = modelBounds.getSize();
	glm::vec3 modelCenter = modelBounds.getCenter();

	// Calculate scale to normalize to ~2 unit dimensions
	const float maxDimension = std::max({modelSize.x, modelSize.y, modelSize.z});
	if (maxDimension > 0.0f) {
		const float normalizeScale = 2.0f / maxDimension;

		/// Apply normalization transform to root node
		scene::Transform normalizedTransform;
		normalizedTransform.position = -modelCenter * normalizeScale;  /// Center the model
		normalizedTransform.scale = glm::vec3(normalizeScale);         /// Scale to ~2 units

		rootNode->setLocalTransform(normalizedTransform);

		spdlog::info("Normalized model from bounds min=({},{},{}), max=({},{},{})",
			modelBounds.getMin().x, modelBounds.getMin().y, modelBounds.getMin().z,
			modelBounds.getMax().x, modelBounds.getMax().y, modelBounds.getMax().z);
	}
}

void GltfModelLoader::collectNodeBounds(
	const std::shared_ptr<scene::SceneNode>& node,
	scene::BoundingBox& bounds,
	const glm::mat4& parentTransform) {

	/// Get node's local transform
	glm::mat4 localTransform = node->getLocalTransform().toMatrix();
	glm::mat4 worldTransform = parentTransform * localTransform;

	/// If this node has a mesh, add its bounds
	if (auto mesh = node->getMesh()) {
		// Calculate bounds from vertices
		scene::BoundingBox meshBounds;
		for (const auto& vertex : mesh->getVertices()) {
			// Transform vertex to world space
			glm::vec4 worldPos = worldTransform * glm::vec4(vertex.position, 1.0f);
			meshBounds.addPoint(glm::vec3(worldPos));
		}

		/// Add to overall bounds
		if (meshBounds.isValid()) {
			for (const auto& corner : meshBounds.getCorners()) {
				bounds.addPoint(corner);
			}
		}
	}

	/// Recursively process children
	for (const auto& child : node->getChildren()) {
		collectNodeBounds(child, bounds, worldTransform);
	}
}

} /// namespace lillugsi::rendering