# LillUgsi General Index

This file provides a high-level overview of all source files in the LillUgsi renderer project, organized by directory.

## Detailed Index Files

For comprehensive documentation of classes, functions, and methods, refer to these category-specific detailed index files:

- **@detailed_index_core.md** - Core application, game loop, and time management (3 files)
- **@detailed_index_vulkan.md** - Vulkan API abstraction layer (30 files)
- **@detailed_index_rendering.md** - Rendering system: renderer, cameras, materials, meshes, textures, lights (48 files)
- **detailed_index_models.md** - Model loading system: glTF loader, extractors, material mapping (14 files)
- **detailed_index_scene.md** - Scene graph and spatial management (8 files)
- **detailed_index_planet.md** - Procedural planet generation module (16 files)
- **@detailed_index_shaders.md** - GLSL shader programs (8 files)

---

## src/core/ - Application Layer

- **src/core/application.h** - Main application class managing game loop, time management, and SDL window lifecycle
- **src/core/application.cpp** - Application implementation with fixed timestep updates and event handling
- **src/main.cpp** - Entry point for the application with error handling and initialization

## src/vulkan/ - Vulkan API Abstraction Layer

### Core Vulkan Management

- **src/vulkan/vulkancontext.h** - Centralized Vulkan state manager coordinating instance, device, surface, and swap chain
- **src/vulkan/vulkancontext.cpp** - VulkanContext implementation for initializing Vulkan resources
- **src/vulkan/vulkaninstance.h** - Vulkan instance creation with validation layers and debug messenger
- **src/vulkan/vulkaninstance.cpp** - Instance initialization and validation layer setup
- **src/vulkan/vulkandevice.h** - Logical device creation and queue family management
- **src/vulkan/vulkandevice.cpp** - Device initialization and queue retrieval
- **src/vulkan/vulkanswapchain.h** - Swap chain management for presenting rendered images
- **src/vulkan/vulkanswapchain.cpp** - Swap chain creation with format, present mode, and extent selection

### Resource Management

- **src/vulkan/vulkanbuffer.h** - Generic GPU buffer creation and copy operations
- **src/vulkan/vulkanbuffer.cpp** - Buffer implementation with memory allocation and transfers
- **src/vulkan/buffer.h** - Base RAII buffer wrapper with map/unmap operations
- **src/vulkan/vertexbuffer.h** - Specialized vertex buffer with vertex count tracking
- **src/vulkan/indexbuffer.h** - Specialized index buffer with index type management
- **src/vulkan/depthbuffer.h** - Depth buffer creation and format selection for z-testing
- **src/vulkan/depthbuffer.cpp** - Depth buffer implementation with optimal format finding

### Pipeline and Shader Management

- **src/vulkan/pipelinemanager.h** - Pipeline creation and caching with material-based configuration
- **src/vulkan/pipelinemanager.cpp** - Pipeline manager implementation with shader program reuse
- **src/vulkan/pipelineconfig.h** - Graphics pipeline configuration builder
- **src/vulkan/pipelineconfig.cpp** - Pipeline configuration with state setup and hashing
- **src/vulkan/shadermodule.h** - SPIR-V shader module loading and lifecycle
- **src/vulkan/shadermodule.cpp** - Shader module creation from SPIR-V files
- **src/vulkan/shaderprogram.h** - Shader stage grouping for graphics pipelines
- **src/vulkan/shaderprogram.cpp** - Shader program creation with vertex and fragment stages

### Command and Frame Management

- **src/vulkan/commandbuffermanager.h** - Command pool and buffer allocation with one-time command utilities
- **src/vulkan/commandbuffermanager.cpp** - Command buffer manager implementation with resource tracking
- **src/vulkan/framebuffermanager.h** - Framebuffer creation and recreation for swap chain images
- **src/vulkan/framebuffermanager.cpp** - Framebuffer manager implementation with validation

### Utility Headers

- **src/vulkan/vulkanhandle.h** - RAII wrapper template for Vulkan handle types
- **src/vulkan/vulkanwrappers.h** - Type aliases for common Vulkan RAII handles
- **src/vulkan/vulkanexception.h** - Custom exception class for Vulkan errors
- **src/vulkan/vulkanformatters.h** - Spdlog formatters for Vulkan enums and types
- **src/vulkan/vulkanutils.h** - Utility functions for Vulkan operations

## src/rendering/ - Rendering System

### Core Renderer

- **src/rendering/renderer.h** - Main renderer coordinating Vulkan context, scene, cameras, and materials. Uses Reverse-Z depth buffering
- **src/rendering/renderer.cpp** - Renderer implementation with draw loop, uniform updates, and model loading

### Cameras

- **src/rendering/camera.h** - Base camera class with quaternion-based orientation
- **src/rendering/camera.cpp** - Camera implementation with projection setup
- **src/rendering/editorcamera.h** - Free-movement first-person camera for scene editing
- **src/rendering/editorcamera.cpp** - Editor camera implementation with WASD movement
- **src/rendering/orbitcamera.h** - Object-centric orbiting camera for model viewing
- **src/rendering/orbitcamera.cpp** - Orbit camera implementation with distance and angle control

### Materials

- **src/rendering/material.h** - Base material class defining shader paths and pipeline configuration
- **src/rendering/material.cpp** - Material base implementation with descriptor pool and pipeline setup
- **src/rendering/pbrmaterial.h** - PBR material with metallic-roughness workflow and texture support
- **src/rendering/pbrmaterial.cpp** - PBR material implementation with texture binding and uniform updates
- **src/rendering/debugmaterial.h** - Debug visualization material for normals, colors, and winding order
- **src/rendering/debugmaterial.cpp** - Debug material implementation with visualization modes
- **src/rendering/wireframematerial.h** - Wireframe rendering material for topology visualization
- **src/rendering/wireframematerial.cpp** - Wireframe material implementation
- **src/rendering/custommaterial.h** - User-defined material with custom shader support
- **src/rendering/custommaterial.cpp** - Custom material implementation with flexible uniform buffers
- **src/rendering/terrainmaterial.h** - Height-based biome visualization for planetary terrain
- **src/rendering/terrainmaterial.cpp** - Terrain material with biome blending and noise parameters
- **src/rendering/materialmanager.h** - Centralized material creation and lifecycle management
- **src/rendering/materialmanager.cpp** - Material manager implementation with caching
- **src/rendering/materialtype.h** - Material type enumeration and feature flags
- **src/rendering/shadertype.h** - Shader paths structure for vertex and fragment shaders

### Meshes

- **src/rendering/mesh.h** - Base mesh class with vertex/index data and GPU buffer management
- **src/rendering/cubemesh.h** - Procedural cube mesh with face coloring
- **src/rendering/cubemesh.cpp** - Cube mesh generation implementation
- **src/rendering/icospheremesh.h** - Sphere generation via icosahedron subdivision
- **src/rendering/icospheremesh.cpp** - Icosphere mesh generation with vertex transforms
- **src/rendering/modelmesh.h** - Mesh container for loaded model geometry
- **src/rendering/meshmanager.h** - Mesh creation and GPU buffer management
- **src/rendering/meshmanager.cpp** - Mesh manager implementation with template methods
- **src/rendering/vertex.h** - Vertex structure with position, normal, tangent, color, and UV
- **src/rendering/tangentcalculator.h** - Tangent space calculation for normal mapping

### Textures

- **src/rendering/texture.h** - GPU texture resource with image, memory, view, and sampler
- **src/rendering/texture.cpp** - Texture implementation with upload, mipmap generation, and layout transitions
- **src/rendering/texturemanager.h** - Texture loading, caching, and lifecycle management
- **src/rendering/texturemanager.cpp** - Texture manager implementation with default textures
- **src/rendering/textureloader.h** - Image loading from files and memory buffers
- **src/rendering/textureloader.cpp** - Texture loader implementation using STB image

### Lights

- **src/rendering/light.h** - Light types and GPU-aligned light data structures
- **src/rendering/light.cpp** - Light implementation with directional light support
- **src/rendering/lightmanager.h** - Centralized light management with capacity limits
- **src/rendering/lightmanager.cpp** - Light manager implementation

### Buffers

- **src/rendering/buffermanager.h** - Centralized GPU buffer creation and management
- **src/rendering/buffermanager.cpp** - Buffer manager implementation with staging buffers
- **src/rendering/buffercache.h** - GPU buffer reuse via power-of-2 bucketing
- **src/rendering/buffercache.cpp** - Buffer cache implementation

### Utilities

- **src/rendering/screenshot.h** - Screenshot capture to PNG files
- **src/rendering/screenshot.cpp** - Screenshot implementation with format conversion
- **src/rendering/pipelinefactory.h** - Pipeline creation for model materials
- **src/rendering/pipelinefactory.cpp** - Pipeline factory implementation with feature extraction

## src/rendering/models/ - Model Loading System

### Core Model Loading

- **src/rendering/models/modelloader.h** - Abstract interface for different model formats
- **src/rendering/models/modelmanager.h** - Centralized model loading with caching and async support
- **src/rendering/models/modelmanager.cpp** - Model manager implementation with format registration
- **src/rendering/models/modeldata.h** - Intermediate representation for model data during loading
- **src/rendering/models/gltfmodelloader.h** - glTF/GLB format loading with TinyGLTF
- **src/rendering/models/gltfmodelloader.cpp** - glTF loader implementation with mesh and material extraction

### Data Extraction

- **src/rendering/models/meshextractor.h** - Extract mesh geometry from glTF primitives
- **src/rendering/models/meshextractor.cpp** - Mesh extractor implementation with attribute handling
- **src/rendering/models/materialextractor.h** - Extract material properties from glTF materials
- **src/rendering/models/materialextractor.cpp** - Material extractor implementation with PBR workflows
- **src/rendering/models/embeddedtextureextractor.h** - Extract and register embedded glTF textures
- **src/rendering/models/embeddedtextureextractor.cpp** - Embedded texture extraction from buffer views

### Processing Pipelines

- **src/rendering/models/materialparametermapper.h** - Map model material data to engine PBR materials
- **src/rendering/models/materialparametermapper.cpp** - Material mapper with texture loading
- **src/rendering/models/textureloadingpipeline.h** - Async texture loading with caching
- **src/rendering/models/textureloadingpipeline.cpp** - Texture loading pipeline implementation
- **src/rendering/models/scenegraphconstructor.h** - Build scene hierarchy from glTF node structure
- **src/rendering/models/scenegraphconstructor.cpp** - Scene graph construction with transform handling

## src/scene/ - Scene Graph and Spatial Management

- **src/scene/scenetypes.h** - Fundamental types for scene system (NodeID, Transform, BoundsType)
- **src/scene/scenetypes.cpp** - Scene types implementation
- **src/scene/scenenode.h** - Hierarchical scene graph nodes with transform propagation
- **src/scene/scenenode.cpp** - Scene node implementation with parent-child relationships
- **src/scene/scene.h** - Top-level scene manager coordinating nodes and culling
- **src/scene/scene.cpp** - Scene implementation with render data collection
- **src/scene/frustum.h** - View frustum for efficient culling calculations
- **src/scene/frustum.cpp** - Frustum implementation with intersection tests
- **src/scene/boundingbox.h** - Axis-aligned bounding box for collision and culling
- **src/scene/boundingbox.cpp** - Bounding box implementation with transformations

## modules/planet/ - Procedural Planet Generation (Optional Module)

### Core Planet System

- **modules/planet/src/planet/planetgenerator.h** - Terrain generation and modification interface
- **modules/planet/src/planet/planetgenerator.cpp** - Planet generator implementation with noise
- **modules/planet/src/planet/planetdata.h** - Planet mesh data structure and visitor pattern support
- **modules/planet/src/planet/planetdata.cpp** - Planet data implementation with subdivision and queries
- **modules/planet/src/planet/face.h** - Triangle face in planet mesh with subdivision hierarchy
- **modules/planet/src/planet/face.cpp** - Face implementation with neighbor relationships
- **modules/planet/src/planet/vertexdata.h** - Vertex with elevation, normal, and slope caching
- **modules/planet/src/planet/vertexdata.cpp** - Vertex data implementation with lazy calculations

### Visitor Pattern

- **modules/planet/src/planet/vertexvisitor.h** - Abstract visitor for vertex operations
- **modules/planet/src/planet/noiseterrainvisitor.h** - Apply noise-based terrain modifications
- **modules/planet/src/planet/noiseterrainvisitor.cpp** - Noise visitor implementation
- **modules/planet/src/planet/terraingeneratorvisitor.h** - Generate terrain from planet data
- **modules/planet/src/planet/terraingeneratorvisitor.cpp** - Terrain generator visitor
- **modules/planet/src/planet/datasettingvisitor.h** - Set data on planet faces
- **modules/planet/src/planet/datasettingvisitor.cpp** - Data setting visitor
- **modules/planet/src/planet/heightdifferencevisitor.h** - Calculate height differences

### Utilities

- **modules/planet/src/planet/planetformater.h** - Formatting utilities for planet data
- **modules/planet/src/planet/main.cpp** - Planet module standalone entry point

## shaders/ - GLSL Shader Programs (9 files)

- **shaders/pbr.glsl.vert** - PBR vertex shader with Reverse-Z depth and tangent space
- **shaders/pbr.glsl.frag** - PBR fragment shader with metallic-roughness workflow and normal mapping
- **shaders/debug.glsl.vert** - Debug visualization vertex shader
- **shaders/debug.glsl.frag** - Debug fragment shader for normals, colors, and winding order
- **shaders/normal_debug.glsl.frag** - Advanced normal mapping debug shader with 21 visualization modes
- **shaders/wireframe.glsl.vert** - Wireframe rendering vertex shader
- **shaders/wireframe.glsl.frag** - Wireframe rendering fragment shader
- **shaders/terrain.glsl.vert** - Terrain rendering vertex shader with height-based positioning
- **shaders/terrain.glsl.frag** - Terrain fragment shader with biome blending

---

## Summary

- **Core Application**: 3 files
- **Vulkan Abstraction**: 30 files
- **Rendering System**: 48 files (including materials, meshes, textures, lights)
- **Model Loading**: 14 files
- **Scene Management**: 8 files
- **Planet Module**: 16 files (optional)
- **Shaders**: 9 files

**Total**: ~128 source and shader files (excluding external dependencies)
