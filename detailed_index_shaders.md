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
