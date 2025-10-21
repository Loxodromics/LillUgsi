# LillUgsi Model Loading System - Detailed Index

This file contains detailed information about the model loading and processing classes.

---

## src/rendering/models/modeldata.h

### Struct: `ModelMeshData`
Intermediate mesh data.

**Members:**
- `std::vector<Vertex> vertices`
- `std::vector<uint32_t> indices`
- `std::string materialName`

### Struct: `ModelData::MaterialInfo`
PBR material properties.

**Members:**
- `glm::vec4 baseColor`
- `float roughness`, `metallic`
- `std::string albedoTexture`, `normalTexture`, `roughnessTexture`, `metallicTexture`, `occlusionTexture`
- `bool hasTransparency`
- `float alphaMode`

### Struct: `ModelData::NodeInfo`
Hierarchy node data.

**Members:**
- `glm::mat4 transform`
- `std::vector<size_t> childIndices`
- `int meshIndex`

### Struct: `ModelData`
Complete model representation.

**Members:**
- `std::vector<ModelMeshData> meshes`
- `std::unordered_map<std::string, MaterialInfo> materials`
- `std::vector<NodeInfo> nodes`
- `size_t rootNodeIndex`

---

## src/rendering/models/modelloader.h

### Struct: `ModelLoadOptions`
Model loading configuration.

**Members:**
- `bool calculateTangents`
- `bool generateMipmaps`
- `bool loadAnimations`
- `float scale`

### Class: `ModelLoader` (abstract)
Abstract model format interface.

**Public Methods:**
- `virtual std::shared_ptr<scene::SceneNode> loadModel(const std::string& filePath, scene::Scene* scene, std::shared_ptr<scene::SceneNode> parentNode, const ModelLoadOptions& options) = 0` - Load model (pure virtual)
- `virtual bool supportsFormat(const std::string& extension) const = 0` - Format detection (pure virtual)

---

## src/rendering/models/modelmanager.h

### Class: `ModelManager`
Centralized model loading with caching.

**Public Methods:**
- `void registerLoader(std::unique_ptr<ModelLoader> loader)` - Register format loader
- `void initialize()` - Register default loaders
- `std::shared_ptr<scene::SceneNode> loadModel(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode, const ModelLoadOptions& options)` - Load synchronously
- `std::future<std::shared_ptr<scene::SceneNode>> loadModelAsync(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode, const ModelLoadOptions& options)` - Load asynchronously
- `bool isLoadingAsync() const` - Check async status
- `void waitForAsyncOperations()` - Wait for all loads
- `bool isModelLoaded(const std::string& filePath) const` - Check cache
- `std::shared_ptr<scene::SceneNode> instantiateModel(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode)` - Instantiate from cache
- `void unloadModel(const std::string& filePath)` - Remove from cache
- `void clearCache()` - Clear all cached models
- `void setResourceBaseDirectory(const std::string& dir)` - Set resource path

---

## src/rendering/models/gltfmodelloader.h

### Class: `GltfModelLoader : ModelLoader`
glTF/GLB format loader.

**Public Methods:**
- `std::shared_ptr<scene::SceneNode> loadModel(...) override` - Load glTF (override)
- `bool supportsFormat(const std::string& extension) const override` - Check .gltf/.glb (override)

**Private Methods:**
- `ModelData parseGltfModel(const tinygltf::Model& gltfModel, const ModelLoadOptions& options)` - Convert to ModelData
- `ModelMeshData extractMeshData(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4& transform)` - Extract mesh
- `ModelData::MaterialInfo extractMaterialInfo(const tinygltf::Model&, int materialIndex)` - Extract material
- `std::vector<std::shared_ptr<Material>> createMaterials(const ModelData&)` - Build materials
- `std::vector<std::shared_ptr<Mesh>> createMeshes(const ModelData&, const std::vector<std::shared_ptr<Material>>&)` - Build meshes
- `template<typename T> std::vector<T> getAccessorData(const tinygltf::Model&, int accessorIndex)` - Get buffer data
- `std::string getTexturePath(const tinygltf::Model&, int textureIndex)` - Resolve texture
- `void normalizeModelTransform(ModelData& modelData)` - Fit to viewport
- `BoundingBox collectNodeBounds(const ModelData&, size_t nodeIndex, const glm::mat4& parentTransform)` - Accumulate bounds

---

## src/rendering/models/meshextractor.h

### Class: `MeshExtractor`
Extract mesh geometry from glTF primitives.

**Public Methods:**
- `static ModelMeshData extractMeshData(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4& transform)` - Main extraction

**Private Static Methods:**
- `static std::vector<glm::vec3> extractPositions(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4&)` - Extract positions
- `static std::vector<glm::vec3> extractNormals(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4&)` - Extract normals
- `static std::vector<glm::vec2> extractTextureCoords(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract UVs
- `static std::vector<glm::vec3> extractColors(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract colors
- `static std::vector<glm::vec3> extractTangents(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract tangents
- `static std::vector<uint32_t> extractIndices(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract indices
- `static template<typename T> std::vector<T> getAccessorData(const tinygltf::Model&, int accessorIndex)` - Raw buffer access
- `static size_t getComponentSize(int componentType)` - Type size
- `static size_t getComponentCount(int type)` - Type count
- `static bool validatePrimitiveTopology(const tinygltf::Primitive&)` - Topology check

---

## src/rendering/models/materialextractor.h

### Class: `MaterialExtractor`
Extract material properties from glTF.

**Public Methods:**
- `void setEmbeddedTextureExtractor(EmbeddedTextureExtractor* extractor)` - Set texture extractor
- `ModelData::MaterialInfo extractMaterialInfo(const tinygltf::Model&, const tinygltf::Material&)` - Extract material
- `std::string getMaterialName(const tinygltf::Material&, int index)` - Generate name
- `std::unordered_map<std::string, ModelData::MaterialInfo> extractAllMaterials(const tinygltf::Model&)` - Batch extraction

**Private Methods:**
- `std::string getTexturePath(const tinygltf::Model&, int textureIndex)` - Resolve texture
- `void extractEmissiveProperties(const tinygltf::Material&, ModelData::MaterialInfo&)` - Extract emissive
- `void extractTransparencyProperties(const tinygltf::Material&, ModelData::MaterialInfo&)` - Extract transparency

---

## src/rendering/models/embeddedtextureextractor.h

### Class: `EmbeddedTextureExtractor`
Extract embedded glTF textures.

**Public Methods:**
- `void extractTextures(const tinygltf::Model& model)` - Extract all images
- `std::string getTextureName(int textureIndex) const` - Get mapped name
- `bool hasTexture(int textureIndex) const` - Check extraction

**Private Methods:**
- `void extractImage(const tinygltf::Image& image, int imageIndex)` - Extract single image
- `std::string generateTextureName(int imageIndex, const std::string& name)` - Generate name
- `VkFormat determineTextureFormat(const std::string& mimeType, int channels)` - MIME to format

---

## src/rendering/models/materialparametermapper.h

### Class: `MaterialParameterMapper`
Map model material to engine PBR material.

**Public Methods:**
- `void applyParameters(const ModelData::MaterialInfo& materialInfo, PBRMaterial* material)` - Apply all properties

**Private Methods:**
- `void applyScalarParameters(const ModelData::MaterialInfo&, PBRMaterial*)` - Apply scalars
- `void applyTextures(const ModelData::MaterialInfo&, PBRMaterial*)` - Load textures
- `void applyAlbedoTexture(const std::string& path, PBRMaterial*)` - Apply albedo
- `void applyNormalTexture(const std::string& path, PBRMaterial*)` - Apply normal
- `void applyRoughnessTexture(const std::string& path, PBRMaterial*)` - Apply roughness
- `void applyMetallicTexture(const std::string& path, PBRMaterial*)` - Apply metallic
- `void applyOcclusionTexture(const std::string& path, PBRMaterial*)` - Apply occlusion
- `bool isSameTexture(const std::string& path1, const std::string& path2)` - Detect packed textures
- `std::shared_ptr<Texture> resolveTexture(const std::string& path)` - Unified loading
- `bool isEmbeddedTexture(const std::string& path)` - Check embedded
- `std::string resolveTexturePath(const std::string& path)` - Path resolution

---

## src/rendering/models/scenegraphconstructor.h

### Class: `SceneGraphConstructor`
Build scene hierarchy from glTF nodes.

**Public Methods:**
- `static std::shared_ptr<scene::SceneNode> buildSceneGraph(const ModelData& modelData, const std::vector<std::shared_ptr<Mesh>>& meshes, scene::Scene* scene, std::shared_ptr<scene::SceneNode> parentNode)` - Build scene graph

**Private Static Methods:**
- `static std::shared_ptr<scene::SceneNode> processNode(const ModelData&, size_t nodeIndex, const std::vector<std::shared_ptr<Mesh>>&, scene::Scene*, std::shared_ptr<scene::SceneNode> parent)` - Process node recursively
- `static void applyNodeTransform(std::shared_ptr<scene::SceneNode> node, const glm::mat4& transform)` - Apply transform
- `static void assignNodeMesh(std::shared_ptr<scene::SceneNode> node, int meshIndex, const std::vector<std::shared_ptr<Mesh>>&)` - Assign mesh
- `static void handlePrimitiveGroups(...)` - Handle multi-primitive meshes

---

## src/rendering/models/textureloadingpipeline.h

### Struct: `TextureLoadOptions`
Texture loading configuration.

**Members:**
- `bool generateMipmaps`
- `bool enableAnisotropicFiltering`
- `bool convertToSRGB`
- `bool enableCaching`

### Class: `TextureLoadingPipeline`
Async texture loading with caching.

**Public Methods:**
- `void setBaseDirectory(const std::string& dir)` - Set base path
- `std::string getBaseDirectory() const` - Get base path
- `std::future<std::shared_ptr<Texture>> requestTextureAsync(const std::string& filePath, const TextureLoadOptions& options)` - Queue async load
- `std::shared_ptr<Texture> loadTexture(const std::string& filePath, const TextureLoadOptions& options)` - Synchronous load
- `bool isTextureLoading(const std::string& filePath) const` - Check async status
- `void waitForAll()` - Wait for all loads
- `void processCompletedOperations()` - Clean up finished tasks
- `void setDefaultOptions(const TextureLoadOptions& options)` - Set global defaults
- `TextureLoadOptions getDefaultOptions() const` - Get defaults
- `size_t getPendingOperationCount() const` - Get pending count

---
