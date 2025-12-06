# LillUgsi Shaders - Detailed Index

This file contains detailed information about the GLSL shader programs.

---

## shaders/pbr.glsl.vert

**Description:** PBR vertex shader with Reverse-Z depth and tangent space calculation.

**Inputs:**
- Vertex attributes: position, normal, tangent, color, texCoord

**Outputs:**
- Transformed position (Reverse-Z)
- World position, normal, tangent, bitangent
- Texture coordinates

**Uniform Blocks:**
- CameraUBO (set=0, binding=0): view, projection, cameraPos
- MaterialUBO (set=2, binding=0): PBR properties including tiling

---

## shaders/pbr.glsl.frag

**Description:** PBR fragment shader with metallic-roughness workflow and normal mapping.

**Features:**
- Cook-Torrance BRDF
- Normal mapping with tangent space
- Metallic-roughness workflow
- Combined texture support (roughness-metallic, ORM)
- Multiple lights support (up to 16)

**Inputs:**
- Fragment position, normal, tangent, bitangent
- Texture coordinates

**Outputs:**
- Final color with PBR lighting

**Uniform Blocks:**
- CameraUBO (set=0): view, projection, cameraPos
- LightUBO (set=1): light array, light count
- MaterialUBO (set=2): PBR properties

**Samplers:**
- albedoTexture, normalTexture, roughnessTexture, metallicTexture, occlusionTexture
- roughnessMetallicTexture, occlusionRoughnessMetallicTexture

---

## shaders/debug.glsl.vert

**Description:** Debug visualization vertex shader.

**Inputs:**
- Vertex attributes: position, normal, color

**Outputs:**
- Transformed position
- World normal
- Vertex color

---

## shaders/debug.glsl.frag

**Description:** Debug fragment shader for normals, colors, and winding order visualization.

**Features:**
- Visualization modes: vertex colors, normal colors, winding order
- Color multiplier for tinting

**Inputs:**
- World normal
- Vertex color

**Outputs:**
- Visualization color

---

## shaders/normal_debug.glsl.frag

**Description:** Advanced debug shader for validating normal mapping implementation with extensive visualization modes.

**Features:**
- 21 distinct visualization modes for debugging normal mapping
- TBN matrix validation (orthogonality, normalization)
- Normal map sampling and transformation visualization
- Lighting comparisons (with/without normal mapping)
- UV coordinate visualization
- View-dependent effects

**Visualization Modes:**
- Mode 0: Normal rendering with Lambert lighting
- Mode 1: Vertex normals (world space)
- Mode 2-4: TBN components (tangent, bitangent, normal)
- Mode 5: Raw normal map sample (as RGB texture)
- Mode 6: Normal map in tangent space
- Mode 7: Final world-space normal after TBN transform
- Mode 8: UV coordinates
- Mode 9-10: Lighting comparison (vertex normals vs mapped normals)
- Mode 11: Difference between vertex and mapped normals
- Mode 12-13: TBN orthogonality and length validation
- Mode 14-16: Individual TBN dot product visualizations
- Mode 17: Face direction check
- Mode 18: View-dependent rim lighting
- Mode 19: Tangent space visualization
- Mode 20: Normal map strength parameter visualization

**Inputs:**
- Fragment position, normal, color
- Texture coordinates
- TBN matrix (mat3)
- View direction

**Outputs:**
- Visualization color based on debug mode

**Uniform Blocks:**
- CameraUBO (set=0): view, projection, cameraPos
- MaterialUBO (set=2): PBR properties, debug mode selector

**Samplers:**
- albedoTexture, normalTexture

---

## shaders/wireframe.glsl.vert

**Description:** Wireframe rendering vertex shader.

**Inputs:**
- Vertex position

**Outputs:**
- Transformed position

---

## shaders/wireframe.glsl.frag

**Description:** Wireframe rendering fragment shader.

**Outputs:**
- Solid wireframe color

---

## shaders/terrain.glsl.vert

**Description:** Terrain rendering vertex shader with height-based positioning.

**Inputs:**
- Vertex attributes: position, normal, texCoord

**Outputs:**
- Transformed position
- World position, normal
- Height for biome blending

---

## shaders/terrain.glsl.frag

**Description:** Terrain fragment shader with biome blending based on height and steepness.

**Features:**
- Multiple biome support (up to 5)
- Height-based color blending
- Steepness-based cliff rendering
- Debug visualization modes

**Inputs:**
- World position, normal
- Height value

**Outputs:**
- Biome-blended color with PBR properties

**Uniform Blocks:**
- TerrainUBO: biome parameters, planet radius, debug mode

---

**End of Shader Index**
