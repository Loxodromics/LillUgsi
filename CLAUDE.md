# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LillUgsi is a C++ Vulkan learning renderer built for educational purposes. It features a modular architecture with modern C++ practices, physically-based rendering (PBR), and a comprehensive scene management system.

## Coding Style Guidelines

- Use camelCase for methods and variables, PascalCase for classes and enums
- Use kPascalCase for constants
- Use all lowercase letters for filenames without underscores or hyphens
- Use lowercase with underscores for namespaces
- Follow Qt's coding style guidelines, but access all members via this-> instead of prefixing with m_
- Use "pointer-to-type" style
- Use K&R brace placement
- Place implementation (.cpp) and header (.h) files next to each other in the src directory
- Use three slashes (///) for comments to differentiate from commented code
- Use tabs for indentation with 4-space tab stops
- Comments should explain complex algorithms, business decisions, and non-obvious code patterns. Do not comment self-explanatory code like simple getters, setters, or obvious operations. Focus on WHY the code exists, not WHAT it does. If a function name clearly describes what it does, additional comments are usually unnecessary
- Comments are written saying 'we' instead of 'I' or passive voice
- Use descriptive names for classes, variables, and functions
- Use const and constexpr for variables and methods where applicable
- Write modern C++ code (C++20) with modern features
- Discourage auto usage except where it genuinely improves readability
- Prefer std::unique_ptr by default for managed pointers. Use std::shared_ptr only when you genuinely need shared ownership semantics
- Use [[nodiscard]] for functions where ignoring the return value would be an error (validation functions, resource acquisition). Avoid for simple getters or functions where the return value is commonly ignored

## Build System & Development Commands

### Dependencies
- Install dependencies using Conan: `conan install . --output-folder=build --build=missing`
- Configure CMake: `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake`
- Build project: `cmake --build build`
- Run executable: `./build/LillUgsi`

### Shader Compilation
Shaders are automatically compiled during build via the `Shaders` CMake target. Manual compilation:
```bash
glslc shaders/pbr.glsl.vert -o build/shaders/pbr.vert.spv
glslc shaders/pbr.glsl.frag -o build/shaders/pbr.frag.spv
```

### Build Configurations
- Debug: Default configuration with debug symbols
- Release: Use `-DCMAKE_BUILD_TYPE=Release` for optimized builds
- Planet Module: Optional module controlled by `USE_PLANET` CMake option

## Code Architecture

### Core Structure
- `src/core/` - Application lifecycle and main loop management
- `src/vulkan/` - Vulkan API abstraction layer with RAII wrappers
- `src/rendering/` - High-level rendering components (renderer, materials, meshes)
- `src/scene/` - Scene graph and spatial management
- `modules/` - Optional feature modules (e.g., planet generation)

### Key Components

#### Application Layer (`src/core/application.h`)
- Main application class with time management and game loop
- Handles SDL window, events, and coordinates with renderer
- Implements fixed timestep updates for consistent simulation

#### Vulkan Abstraction (`src/vulkan/vulkancontext.h`)
- Centralized Vulkan state management
- RAII-based resource management with smart pointers
- Handles instance, device, surface, and swap chain creation

#### Renderer (`src/rendering/renderer.h`)
- Main rendering coordinator using Reverse-Z depth buffering
- Manages pipelines, materials, meshes, and scene rendering
- Supports both editor and orbit camera modes
- Includes model loading with gltf support

### Material System
- PBR materials with metallic-roughness workflow
- Texture-based material parameters (albedo, normal, metallic, roughness, AO)
- Material manager for centralized material creation and management
- Pipeline factory for material-specific rendering pipelines

### Scene Management
- Hierarchical scene graph with transform inheritance
- Frustum culling for performance optimization
- Light management system supporting multiple light types
- Model loading with automatic scene graph construction

## Development Guidelines

### Code Style
- CamelCase for methods/variables, PascalCase for classes
- Lowercase filenames without separators
- Use `this->` for all member access
- Pointer-to-type style: `Type* ptr` not `Type *ptr`
- Three slashes `///` for documentation comments
- Tabs for indentation (4 spaces)
- Modern C++ features: `[[nodiscard]]`, `const` correctness, RAII

### Architecture Patterns
- RAII for all resource management
- Smart pointers for automatic memory management
- Manager classes for system coordination
- Factory patterns for object creation
- Layered architecture with clear separation of concerns

### Graphics Programming Notes
- Uses Reverse-Z depth buffering (depth range [1,0], VK_COMPARE_OP_GREATER)
- PBR shading with metallic-roughness workflow
- Texture coordinates follow OpenGL convention (V flipped for glTF compatibility)
- Normal mapping in tangent space
- Async model loading to prevent main thread blocking

### PBR Material Conventions

**Light Intensity Values:**
- Lambert BRDF uses division by PI for energy conservation
- This requires higher light intensity values (e.g., 8.0 instead of 1.0)
- Physically correct but less intuitive than unnormalized values

**Texture Channel Configuration:**
- Default channel indices assume ORM packed textures:
  - Roughness: G channel (index 1)
  - Metallic: B channel (index 2)
  - Occlusion: R channel (index 0)
- Single-channel R8 textures require explicit configuration:
  ```cpp
  material->setRoughnessChannel(Material::TextureChannel::R);
  material->setMetallicChannel(Material::TextureChannel::R);
  material->setOcclusionChannel(Material::TextureChannel::R);
  ```

### Dependencies & Libraries
- Vulkan SDK 1.3.216+ for graphics API
- SDL3 for windowing and input
- GLM for mathematics with experimental extensions enabled
- spdlog for logging
- TinyGLTF for model loading
- FastNoise2 for procedural generation
- STB for image loading

## Testing & Validation
- No formal test framework currently in place
- Validate changes by running the executable and checking visual output
- Use Vulkan validation layers for debugging graphics issues
- Screenshot functionality available for visual regression testing

## Performance Considerations
- Frustum culling implemented for scene optimization
- Reverse-Z depth buffering for improved precision
- Async texture loading to prevent blocking
- Command buffer management for efficient GPU command submission
- Material batching through pipeline management

## Codebase Index

I have provided you with a comprehensive indexing system organized by subsystem:

**General Overview:**
- The file @general_index.md contains a high-level list of all files in the codebase with simple descriptions.

Each detailed index contains comprehensive class, function, and method documentation with signatures and descriptions. This index may or may not be up to date.
