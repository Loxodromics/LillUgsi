# LillUgsi Rendering System - Detailed Index

This file contains detailed information about the rendering system classes and functions.

---

## src/rendering/renderer.h

### Struct: `Renderer::CameraUBO`
Camera uniform buffer data for GPU.

**Members:**
- `glm::mat4 view` - View matrix
- `glm::mat4 projection` - Projection matrix
- `glm::vec3 cameraPos` - Camera position for lighting
- `float padding` - Alignment padding

### Class: `Renderer`
Main renderer coordinating the rendering pipeline (uses Reverse-Z depth buffering).

**Public Methods:**
- `Renderer()` - Constructor
- `~Renderer()` - Destructor
- `bool initialize(SDL_Window* window)` - Initialize with SDL window
- `void cleanup()` - Clean up all Vulkan resources
- `void drawFrame()` - Draw a single frame
- `void update(float deltaTime)` - Update renderer state
- `bool recreateSwapChain(uint32_t newWidth, uint32_t newHeight)` - Recreate swap chain on resize
- `Camera* getCamera()` - Get camera pointer
- `void handleCameraInput(SDL_Window* window, const SDL_Event& event) const` - Handle camera input
- `scene::Scene* getScene()` - Get scene manager
- `LightManager* getLightManager() const` - Get light manager
- `MaterialManager* getMaterialManager() const` - Get material manager
- `std::shared_ptr<scene::SceneNode> loadModel(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode)` - Load model synchronously
- `std::future<std::shared_ptr<scene::SceneNode>> loadModelAsync(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode = nullptr)` - Load model asynchronously
- `bool captureScreenshot(const std::string& filename)` - Capture screenshot to PNG

**Private Methods:**
- `void createCommandBuffers()` - Create command buffers
- `void createRenderPass()` - Create render pass
- `void createFramebuffers()` - Create framebuffers
- `void cleanupFramebuffers()` - Clean up framebuffers
- `VulkanShaderModuleHandle createShaderModule(const std::vector<char>& code)` - Create shader module
- `void createGraphicsPipeline()` - Create graphics pipeline
- `void recordCommandBuffers()` - Record command buffers
- `void createCameraUniformBuffer()` - Create camera UBO
- `void updateCameraUniformBuffer() const` - Update camera UBO
- `void createDescriptorPool()` - Create descriptor pool
- `void createDescriptorSets()` - Create descriptor sets
- `void createSyncObjects()` - Create synchronization objects
- `void cleanupSyncObjects()` - Clean up sync objects
- `void initializeDepthBuffer()` - Initialize depth buffer
- `void initializeScene()` - Initialize scene
- `void createLightUniformBuffer()` - Create light UBO
- `void updateLightUniformBuffer() const` - Update light UBO
- `void initializeMaterials()` - Initialize materials
- `void initializeModelManager()` - Initialize model manager
- `void initializeModelLoadingComponents()` - Initialize material mapper, texture loader, pipeline factory

---

## src/rendering/camera.h

### Class: `Camera` (abstract)
Base class for all camera implementations using quaternion-based orientation.

**Public Methods:**
- `virtual ~Camera()` - Virtual destructor
- `virtual glm::mat4 getViewMatrix() const = 0` - Get view matrix (pure virtual)
- `virtual glm::mat4 getProjectionMatrix(float aspectRatio) const = 0` - Get projection matrix (pure virtual)
- `virtual void update(float deltaTime) = 0` - Update camera state (pure virtual)
- `virtual void setPosition(const glm::vec3& newPosition)` - Set position
- `virtual glm::vec3 getPosition() const` - Get position
- `virtual void setOrientation(const glm::quat& newOrientation)` - Set orientation
- `virtual glm::quat getOrientation() const` - Get orientation
- `virtual void setFov(float newFov)` - Set field of view
- `virtual float getFov() const` - Get field of view
- `virtual void setNearPlane(float newNearPlane)` - Set near clipping plane
- `virtual float getNearPlane() const` - Get near plane
- `virtual void setFarPlane(float newFarPlane)` - Set far clipping plane
- `virtual float getFarPlane() const` - Get far plane

**Protected Methods:**
- `glm::vec3 getFront() const` - Get front direction vector
- `glm::vec3 getUp() const` - Get up direction vector
- `glm::vec3 getRight() const` - Get right direction vector

---

## src/rendering/editorcamera.h

### Class: `EditorCamera : Camera`
Free-movement first-person camera.

**Public Methods:**
- `EditorCamera(const glm::vec3& position, float yaw, float pitch)` - Constructor
- `void handleInput(const SDL_Event& event, SDL_Window* window)` - Handle SDL events
- `void update(float deltaTime)` - Update camera (override)
- `glm::mat4 getViewMatrix() const` - Get view matrix (override)
- `glm::mat4 getProjectionMatrix(float aspectRatio) const` - Get projection (override)
- `void setMovementSpeed(float speed)` - Set movement speed
- `void setMouseSensitivity(float sensitivity)` - Set mouse sensitivity

---

## src/rendering/orbitcamera.h

### Class: `OrbitCamera : Camera`
Object-centric orbiting camera.

**Public Methods:**
- `OrbitCamera(const glm::vec3& targetPoint, float distance, float horizontalAngle, float verticalAngle, bool clampVerticalAngle = true)` - Constructor
- `void handleInput(const SDL_Event& event, SDL_Window* window)` - Handle SDL events
- `void update(float deltaTime)` - Update camera (override)
- `glm::mat4 getViewMatrix() const` - Get view matrix (override)
- `glm::mat4 getProjectionMatrix(float aspectRatio) const` - Get projection (override)
- `void setTargetPoint(const glm::vec3& target)` - Set orbit target
- `glm::vec3 getTargetPoint() const` - Get orbit target
- `void setDistance(float distance)` - Set distance from target
- `float getDistance() const` - Get distance
- `void setMouseSensitivity(float sensitivity)` - Set mouse sensitivity
- `void setZoomSensitivity(float sensitivity)` - Set zoom sensitivity

---

## src/rendering/material.h

### Enum: `Material::CullingMode`
Culling mode configuration.

**Values:**
- `None` - No culling
- `Back` - Back-face culling (default)
- `Front` - Front-face culling (for inverted models)

### Enum: `Material::TextureChannel`
Texture channel selection.

**Values:**
- `R`, `G`, `B`, `A` - Individual channels

### Class: `Material` (abstract, non-copyable)
Base class for all materials.

**Public Methods:**
- `virtual ~Material()` - Virtual destructor
- `virtual ShaderPaths getShaderPaths() const = 0` - Get shader paths (pure virtual)
- `virtual PipelineConfig getPipelineConfig() const` - Get pipeline configuration
- `MaterialType getType() const` - Get material type
- `MaterialFeatureFlags getFeatures() const` - Get feature flags
- `const std::string& getName() const` - Get material name
- `bool hasFeature(MaterialFeatureFlags feature) const` - Check feature enabled
- `void setCullingMode(CullingMode cullingMode)` - Set culling mode
- `virtual void bind(VkCommandBuffer, VkPipelineLayout) const` - Bind for rendering
- `virtual VkDescriptorSetLayout getDescriptorSetLayout() const` - Get descriptor layout

**Protected Methods:**
- `Material(VkDevice, const std::string& name, VkPhysicalDevice, MaterialType type, MaterialFeatureFlags features)` - Constructor
- `PipelineConfig getDefaultConfig() const` - Get default pipeline config
- `virtual bool createDescriptorPool()` - Create descriptor pool
- `virtual void configurePipeline(PipelineConfig& config) const` - Customize pipeline

---

## src/rendering/pbrmaterial.h

### Class: `PBRMaterial : Material`
PBR material with metallic-roughness workflow.

**Public Methods:**
- Texture setters:
  - `void setAlbedoTexture(std::shared_ptr<Texture>)` - Set base color texture
  - `void setNormalMap(std::shared_ptr<Texture>)` - Set normal map
  - `void setRoughnessMap(std::shared_ptr<Texture>)` - Set roughness map
  - `void setMetallicMap(std::shared_ptr<Texture>)` - Set metallic map
  - `void setOcclusionMap(std::shared_ptr<Texture>)` - Set AO map
  - `void setRoughnessMetallicMap(std::shared_ptr<Texture>)` - Set combined roughness-metallic
  - `void setOcclusionRoughnessMetallicMap(std::shared_ptr<Texture>)` - Set ORM map
- Parameter setters:
  - `void setBaseColor(const glm::vec4&)` - Set base color
  - `void setRoughness(float)` - Set roughness value
  - `void setMetallic(float)` - Set metallic value
  - `void setAmbient(float)` - Set ambient occlusion value
  - `void setNormalStrength(float)` - Set normal map strength
  - `void setRoughnessStrength(float)` - Set roughness strength
  - `void setMetallicStrength(float)` - Set metallic strength
  - `void setOcclusionStrength(float)` - Set occlusion strength
  - `void setTextureTiling(float u, float v)` - Set UV tiling
- Getters for all properties

---

## src/rendering/debugmaterial.h

### Enum: `DebugMaterial::VisualizationMode`
Debug visualization modes.

**Values:**
- `VertexColors` - Show vertex colors
- `NormalColors` - Show normals as colors
- `WindingOrder` - Visualize triangle winding

### Class: `DebugMaterial : Material`
Debug visualization material.

**Public Methods:**
- `void setVisualizationMode(VisualizationMode mode)` - Set visualization mode
- `VisualizationMode getVisualizationMode() const` - Get current mode
- `void setColorMultiplier(const glm::vec4& color)` - Set color tint
- `glm::vec4 getColorMultiplier() const` - Get color tint

---

## src/rendering/wireframematerial.h

### Class: `WireframeMaterial : Material`
Wireframe rendering material.

**Public Methods:**
- `void setColor(const glm::vec4& color)` - Set wireframe color
- `glm::vec4 getColor() const` - Get wireframe color

---

## src/rendering/terrainmaterial.h

### Struct: `NoiseParameters`
Noise generation parameters.

**Members:**
- `float frequency`, `amplitude`, `octaves`, `persistence`, `lacunarity`

### Struct: `TransitionParameters`
Biome transition configuration.

**Members:**
- `int type`, `float scale`, `float sharpness`, `NoiseParameters noise`

### Struct: `BiomeParameters`
Biome visual and physical properties.

**Members:**
- `glm::vec3 color`, `cliffColor`
- `float minHeight`, `maxHeight`, `minSteepness`, `roughness`, `metallic`

### Enum: `TerrainMaterial::TerrainDebugMode`
Debug visualization modes.

**Values:**
- `None`, `Height`, `Steepness`, `Normals`, `Tangents`, `Bitangents`, `UVCoordinates`

### Class: `TerrainMaterial : Material`
Height-based biome visualization for planetary terrain.

**Public Methods:**
- `void setBiome(int biomeIndex, const BiomeParameters& params)` - Configure biome
- `void setPlanetRadius(float radius)` - Set planet radius
- `BiomeParameters& getProperties(int biomeIndex)` - Get biome properties
- `void setNoiseParameters(int biomeIndex, const NoiseParameters& noise)` - Set biome noise
- `void setDebugMode(TerrainDebugMode mode)` - Set debug visualization
- `TerrainDebugMode getDebugMode() const` - Get debug mode

---

## src/rendering/custommaterial.h

### Class: `CustomMaterial : Material`
Custom material implementation for user-defined shaders and properties.

---

## src/rendering/materialmanager.h

### Class: `MaterialManager`
Centralized material creation and caching.

**Public Methods:**
- `std::shared_ptr<PBRMaterial> createPBRMaterial(const std::string& name)` - Create or get PBR material
- `std::shared_ptr<CustomMaterial> createCustomMaterial(const std::string& name)` - Create custom material
- `std::shared_ptr<WireframeMaterial> createWireframeMaterial(const std::string& name)` - Create wireframe
- `std::shared_ptr<TerrainMaterial> createTerrainMaterial(const std::string& name)` - Create terrain
- `std::shared_ptr<DebugMaterial> createDebugMaterial(const std::string& name)` - Create debug
- `std::shared_ptr<Material> getMaterial(const std::string& name) const` - Get by name
- `bool hasMaterial(const std::string& name) const` - Check existence
- `const std::unordered_map<std::string, std::shared_ptr<Material>>& getMaterials() const` - Get all
- `void cleanup()` - Clean up resources

---

## src/rendering/mesh.h

### Struct: `Mesh::RenderData`
Everything needed to render a mesh.

**Members:**
- `glm::mat4 modelMatrix` - Transform matrix
- `std::shared_ptr<VertexBuffer> vertexBuffer` - Vertex buffer
- `std::shared_ptr<IndexBuffer> indexBuffer` - Index buffer
- `std::shared_ptr<Material> material` - Material

### Class: `Mesh` (abstract)
Base mesh class with CPU and GPU data.

**Public Methods:**
- `virtual ~Mesh()` - Virtual destructor
- `virtual void generateGeometry() = 0` - Generate mesh geometry (pure virtual)
- `virtual void prepareRenderData(RenderData& data) const` - Populate render data
- `const std::vector<Vertex>& getVertices() const` - Get vertex data
- `const std::vector<uint32_t>& getIndices() const` - Get index data
- `void setBuffers(std::shared_ptr<VertexBuffer>, std::shared_ptr<IndexBuffer>)` - Set GPU buffers
- `void setTranslation(const glm::vec3&)` - Set translation
- `void setMaterial(std::shared_ptr<Material>)` - Set material
- `std::shared_ptr<Material> getMaterial() const` - Get material
- `void markBuffersDirty()` - Mark buffers needing update
- `bool needsBufferUpdate() const` - Check if update needed
- `void clearBuffersDirty()` - Clear dirty flag
- `void setTextureTiling(float u, float v)` - Set texture tiling
- `float getTextureTilingU() const` - Get U tiling
- `float getTextureTilingV() const` - Get V tiling

**Protected Methods:**
- `glm::vec2 applyTextureTiling(const glm::vec2& uv) const` - Apply tiling to UV

---

## src/rendering/cubemesh.h

### Class: `CubeMesh : Mesh`
Procedural cube mesh.

**Public Methods:**
- `explicit CubeMesh(float sideLength = 1.0f)` - Constructor
- `void generateGeometry()` - Generate cube (override)
- `void setFaceColors(const std::array<glm::vec3, 6>& colors)` - Set per-face colors

---

## src/rendering/icospheremesh.h

### Struct: `IcosphereMesh::VertexTransform`
Vertex transformation data.

**Members:**
- `glm::vec3 newPosition`, `newNormal`, `newColor`

### Class: `IcosphereMesh : Mesh`
Sphere via icosahedron subdivision.

**Public Methods:**
- `IcosphereMesh(float radius = 1.0f, int subdivisions = 0)` - Constructor
- `void generateGeometry()` - Generate sphere (override)
- `void applyVertexTransforms(const std::vector<VertexTransform>& transforms)` - Batch vertex updates
- `std::vector<glm::vec3> getVertexPositions() const` - Get positions
- `float getRadius() const` - Get radius
- `int getSubdivisions() const` - Get subdivision level

**Private Methods:**
- `void initializeBaseIcosahedron()` - Create base icosahedron
- `void subdivide()` - Subdivide once
- `uint32_t getOrCreateMidpoint(uint32_t v1, uint32_t v2, std::unordered_map<uint64_t, uint32_t>& midpointCache)` - Get/create midpoint
- `static uint64_t generateEdgeKey(uint32_t v1, uint32_t v2)` - Edge hash

---

## src/rendering/meshmanager.h

### Class: `MeshManager`
Mesh creation and buffer management.

**Public Methods:**
- `template<typename MeshType, typename... Args> std::shared_ptr<MeshType> createMesh(Args&&... args)` - Create procedural mesh
- `template<typename MeshType> std::shared_ptr<MeshType> createMeshWithGeometry(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)` - Create with data
- `void updateBuffers(Mesh* mesh)` - Update GPU buffers
- `void updateBuffersIfNeeded(Mesh* mesh)` - Conditional buffer update

---

## src/rendering/texture.h

### Enum: `Texture::FilterMode`
Texture filtering modes.

**Values:**
- `Nearest`, `Linear`, `Cubic`

### Enum: `Texture::WrapMode`
Texture wrapping modes.

**Values:**
- `Repeat`, `MirroredRepeat`, `ClampToEdge`, `ClampToBorder`

### Class: `Texture`
GPU texture resource with image, view, and sampler.

**Public Methods:**
- `Texture(VkDevice, VkPhysicalDevice, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels, uint32_t layerCount, const std::string& name)` - Constructor
- `~Texture()` - Destructor
- `void uploadData(const void* data, size_t size, VkCommandPool, VkQueue, CommandBufferManager&)` - Upload pixel data
- `void configureSampler(FilterMode min, FilterMode mag, WrapMode wrapU, WrapMode wrapV, bool anisotropy, float maxAnisotropy)` - Configure sampler
- `void generateMipmaps(VkCommandPool, VkQueue, CommandBufferManager&)` - Generate mipmaps
- `VkImageView getImageView() const` - Get image view
- `VkSampler getSampler() const` - Get sampler
- `uint32_t getWidth() const` - Get width
- `uint32_t getHeight() const` - Get height
- `VkFormat getFormat() const` - Get format
- `uint32_t getMipLevels() const` - Get mipmap levels
- `bool hasAlpha() const` - Check alpha channel
- `const std::string& getName() const` - Get name

**Private Methods:**
- `void transitionLayout(CommandBufferManager&, VkCommandPool, VkQueue, VkImageLayout old, VkImageLayout new, uint32_t baseMip, uint32_t levelCount, uint32_t baseLayer, uint32_t layerCount)` - Transition image layout
- `static uint32_t calculateMipLevels(uint32_t width, uint32_t height)` - Calculate mip levels
- `uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const` - Find memory type
- `bool isFormatSupported(VkFormat, VkFormatFeatureFlags) const` - Check format support
- `static VkFilter toVkFilter(FilterMode)` - Convert filter mode
- `static VkSamplerAddressMode toVkAddressMode(WrapMode)` - Convert wrap mode

---

## src/rendering/texturemanager.h

### Class: `TextureManager`
Texture loading, caching, and lifecycle.

**Public Methods:**
- `std::shared_ptr<Texture> getOrLoadTexture(const std::string& name, const std::string& filePath)` - Load with caching
- `std::shared_ptr<Texture> createTexture(const std::string& name, uint32_t width, uint32_t height, const void* data, size_t dataSize, VkFormat format)` - Create from data
- `std::shared_ptr<Texture> createTextureFromBufferView(const std::string& name, const std::vector<unsigned char>& data, int width, int height, int channels)` - Create from buffer view
- `bool isTextureLoaded(const std::string& name) const` - Check cache
- `std::shared_ptr<Texture> getTexture(const std::string& name) const` - Get cached texture
- `void releaseTexture(const std::string& name)` - Release texture
- `void releaseAllTextures()` - Clear cache
- `std::shared_ptr<Texture> createDefaultTexture()` - Create white default
- `std::shared_ptr<Texture> getDefaultTexture() const` - Get default texture

---

## src/rendering/textureloader.h

### Struct: `TextureLoader::TextureData`
Loaded texture data.

**Members:**
- `unsigned char* pixels`
- `int width`, `height`, `channels`
- `bool success`
- `std::string errorMessage`

### Enum: `TextureLoader::Format`
Target pixel formats.

**Values:**
- `Keep`, `RGB`, `RGBA`, `R`, `NormalMap`

### Class: `TextureLoader`
Image loading from files and memory.

**Public Static Methods:**
- `static TextureData loadFromFile(const std::string& filepath, Format desiredFormat)` - Load from file
- `static TextureData loadFromMemory(const unsigned char* buffer, size_t size, Format desiredFormat)` - Load from buffer
- `static TextureData loadFromBufferView(const std::vector<unsigned char>& buffer, Format desiredFormat)` - Load from buffer view

---

## src/rendering/light.h

### Struct: `LightData` (GPU-aligned)
GPU light data structure.

**Members:**
- `glm::vec4 direction` - Light direction
- `glm::vec4 colorAndIntensity` - RGB + intensity
- `glm::vec4 ambient` - Ambient contribution

### Class: `Light` (abstract)
Base light class.

**Public Methods:**
- `virtual ~Light()` - Virtual destructor
- `virtual LightData getLightData() const = 0` - Get GPU data (pure virtual)
- `virtual void setColor(const glm::vec3& color) = 0` - Set color (pure virtual)
- `virtual glm::vec3 getColor() const = 0` - Get color (pure virtual)
- `virtual void setIntensity(float intensity) = 0` - Set intensity (pure virtual)
- `virtual float getIntensity() const = 0` - Get intensity (pure virtual)
- `virtual void setAmbient(const glm::vec3& ambient) = 0` - Set ambient (pure virtual)
- `virtual glm::vec3 getAmbient() const = 0` - Get ambient (pure virtual)

### Class: `DirectionalLight : Light`
Directional light implementation.

**Public Methods:**
- `DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)` - Constructor
- `void setDirection(const glm::vec3& direction)` - Set normalized direction
- `glm::vec3 getDirection() const` - Get direction
- All inherited virtual methods (override)

---

## src/rendering/lightmanager.h

### Class: `LightManager`
Centralized light management.

**Public Constants:**
- `static constexpr size_t MaxLights = 16` - Maximum light count

**Public Methods:**
- `size_t addLight(std::shared_ptr<Light> light)` - Add light, return index
- `void removeLight(size_t index)` - Remove light by index
- `void removeAllLights()` - Clear all lights
- `std::shared_ptr<Light> getLight(size_t index) const` - Get light by index
- `const std::vector<std::shared_ptr<Light>>& getLights() const` - Get all lights
- `size_t getLightCount() const` - Get light count
- `std::vector<LightData> getLightData() const` - Prepare GPU-format data
- `bool canAddLight() const` - Check capacity

---

## src/rendering/buffermanager.h

### Class: `BufferManager`
Centralized GPU buffer creation.

**Public Methods:**
- `bool initialize()` - Initialize command pool
- `void cleanup()` - Clean up resources
- `std::shared_ptr<VertexBuffer> createVertexBuffer(const std::vector<Vertex>& vertices)` - Create vertex buffer
- `std::shared_ptr<IndexBuffer> createIndexBuffer(const std::vector<uint32_t>& indices)` - Create index buffer
- `std::shared_ptr<Buffer> createUniformBuffer(VkDeviceSize size)` - Create uniform buffer
- `std::shared_ptr<Buffer> createStorageBuffer(VkDeviceSize size, const void* data)` - Create storage buffer
- `std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, const void* data)` - Create staging buffer
- `void updateBuffer(std::shared_ptr<Buffer> buffer, const void* data, VkDeviceSize size, VkDeviceSize offset)` - Update buffer
- `void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)` - Copy buffer
- `BufferCache* getBufferCache()` - Get buffer cache

---

## src/rendering/buffercache.h

### Class: `BufferCache`
GPU buffer reuse via bucketing.

**Public Methods:**
- `std::shared_ptr<VertexBuffer> getOrCreateVertexBuffer(const std::vector<Vertex>& vertices, BufferManager* bufferManager)` - Get or create vertex buffer
- `std::shared_ptr<IndexBuffer> getOrCreateIndexBuffer(const std::vector<uint32_t>& indices, BufferManager* bufferManager)` - Get or create index buffer
- `void cleanup()` - Clear cache
- `bool hasActiveBuffers() const` - Check for in-use buffers

**Private Methods:**
- `static size_t calculateBufferBucket(VkDeviceSize size)` - Calculate size bucket

---

## src/rendering/pipelinefactory.h

### Class: `PipelineFactory`
Pipeline creation for model materials.

**Public Methods:**
- `void createPipelinesForModel(const std::vector<std::shared_ptr<Material>>& materials)` - Create pipelines for all materials
- `void createPipelineForMaterial(const std::shared_ptr<Material>& material)` - Create single pipeline
- `bool hasPipeline(const std::string& materialName) const` - Check existence
- `void clearCache()` - Clear pipeline cache

**Private Methods:**
- `void configureMaterialFeatures(const ModelData::MaterialInfo& materialInfo, PBRMaterial* material)` - Extract features
- `void setStandardMaterialParams(const ModelData::MaterialInfo& materialInfo, PBRMaterial* material)` - Map material data

---

## src/rendering/screenshot.h

### Class: `Screenshot`
Screenshot capture to PNG.

**Public Methods:**
- `bool captureScreenshot(VkDevice, VkPhysicalDevice, VkImage, VkFormat, uint32_t width, uint32_t height, VkQueue, VkCommandPool, CommandBufferManager*, const std::string& filename)` - Capture to PNG

**Private Methods:**
- `void transitionImageLayout(VkCommandBuffer, VkImage, VkFormat, VkImageLayout old, VkImageLayout new)` - Transition layout
- `VulkanBufferHandle createScreenshotBuffer(VkDevice, VkPhysicalDevice, VkDeviceSize size, VkDeviceMemory& memory)` - Create staging buffer
- `bool saveScreenshotToPNG(const unsigned char* data, uint32_t width, uint32_t height, const std::string& filename)` - Write PNG
- `void cleanup()` - Clean up resources
- `uint32_t getFormatSize(VkFormat format)` - Get pixel size
- `std::vector<unsigned char> convertToRGBA(const unsigned char* data, uint32_t width, uint32_t height, VkFormat format)` - Format conversion

---

## src/rendering/vertex.h

### Struct: `Vertex`
Vertex structure with geometry and attributes.

**Members:**
- `glm::vec3 position` - Vertex position
- `glm::vec3 normal` - Vertex normal
- `glm::vec3 tangent` - Tangent vector for normal mapping
- `glm::vec3 color` - Vertex color
- `glm::vec2 texCoord` - Texture coordinates

**Static Methods:**
- `static VkVertexInputBindingDescription getBindingDescription()` - Get binding description
- `static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions()` - Get attribute descriptions
- `bool operator==(const Vertex& other) const` - Equality comparison

---

## src/rendering/tangentcalculator.h

### Class: `TangentCalculator` (static only)
Tangent space calculation.

**Public Static Methods:**
- `static void calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)` - Calculate for triangles
- `static void calculateTangentsForQuads(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)` - Calculate for quads

---

**End of Rendering System Index**
