# LillUgsi Planet Module - Detailed Index

This file contains detailed information about the procedural planet generation system.

---

## modules/planet/src/planet/planetgenerator.h

### Struct: `PlanetGenerator::GeneratorSettings`
Terrain noise parameters.

**Members:**
- `float baseFrequency`, `amplitude`
- `int octaves`
- `float persistence`, `lacunarity`
- `int seed`

### Class: `PlanetGenerator`
Terrain generation and modification.

**Public Methods:**
- `PlanetGenerator(std::shared_ptr<PlanetData> planetData, std::shared_ptr<rendering::IcosphereMesh> mesh)` - Constructor
- `void generateTerrain()` - Generate and update mesh
- `void modifyTerrain(const glm::vec3& point, float radius, float strength)` - Modify elevation
- `void setSettings(const GeneratorSettings& settings)` - Set parameters
- `GeneratorSettings getSettings() const` - Get parameters

**Private Methods:**
- `void updateMesh()` - Sync mesh with planet data

---

## modules/planet/src/planet/planetdata.h

### Class: `PlanetData`
Planet mesh data with visitor pattern.

**Public Methods:**
- `void subdivide(int levels)` - Recursive subdivision
- `std::vector<glm::vec3> getVertices() const` - Export vertices
- `std::vector<uint32_t> getIndices() const` - Export indices
- `void applyVisitorToFace(const glm::vec3& point, FaceVisitor& visitor, float radius)` - Apply to face containing point
- `void applyFaceVisitor(FaceVisitor& visitor)` - Apply to all faces
- `void applyVertexVisitor(VertexVisitor& visitor)` - Apply to all vertices
- `std::shared_ptr<Face> getFaceAtPoint(const glm::vec3& point)` - Find face
- `float getHeightAt(const glm::vec3& point)` - Get height at point
- `float getHeightAtNearestVertex(const glm::vec3& point)` - Nearest vertex height
- `float getInterpolatedHeightAt(const glm::vec3& point)` - Smooth height
- `glm::vec3 getNormalAt(const glm::vec3& point)` - Get normal
- `glm::vec3 getNormalAtNearestVertex(const glm::vec3& point)` - Nearest normal
- `glm::vec3 getInterpolatedNormalAt(const glm::vec3& point)` - Smooth normal
- `void updateNormals()` - Recalculate all normals
- `void updateNormalsForVertex(size_t vertexIndex)` - Update single vertex normal
- `void verifyNormalDirections()` - Debug verification

---

## modules/planet/src/planet/face.h

### Struct: `FaceVisitor` (abstract)
Visitor for face operations.

**Public Methods:**
- `virtual void visit(Face& face) = 0` - Visit face (pure virtual)

### Class: `Face`
Triangle face with subdivision hierarchy.

**Public Methods:**
- `Face(size_t v0, size_t v1, size_t v2)` - Constructor
- `void setData(void* data)` - Set generic data
- `void* getData() const` - Get generic data
- `void setNeighbor(int edge, std::shared_ptr<Face> neighbor)` - Set neighbor
- `std::shared_ptr<Face> getNeighbor(int edge) const` - Get neighbor
- `void addChild(std::shared_ptr<Face> child)` - Add subdivision child
- `void setChild(int index, std::shared_ptr<Face> child)` - Set child by index
- `std::shared_ptr<Face> getChild(int index) const` - Get child
- `std::array<std::shared_ptr<Face>, 4> getChildren() const` - Get all 4 children
- `void setParent(std::weak_ptr<Face> parent)` - Set parent
- `std::weak_ptr<Face> getParent() const` - Get parent
- `void setVertexIndices(size_t v0, size_t v1, size_t v2)` - Set vertices
- `std::array<size_t, 3> getVertexIndices() const` - Get vertices
- `glm::vec3 getMidpoint(const std::vector<VertexData>& vertices) const` - Get center
- `glm::vec3 calculateMidpoint(const std::vector<VertexData>& vertices) const` - Calculate center
- `glm::vec3 calculateNormal(const std::vector<VertexData>& vertices) const` - Calculate normal
- `glm::vec3 getNormal() const` - Get normal
- `bool isLeaf() const` - Check if leaf

---

## modules/planet/src/planet/vertexdata.h

### Class: `VertexData` (non-copyable, movable)
Vertex with elevation and slope caching.

**Public Methods:**
- `VertexData(const glm::vec3& position, size_t index)` - Constructor
- `size_t getIndex() const` - Get vertex index
- `float getElevation() const` - Get elevation with dirty tracking
- `void setElevation(float elevation)` - Set elevation
- `glm::vec3 getPosition() const` - Get position
- `glm::vec3 getNormal() const` - Get normal with recalculation
- `void setNormal(const glm::vec3& normal)` - Set normal
- `glm::vec3 getNormal() const` - Get normal
- `void addNeighbor(size_t neighborIndex)` - Add neighbor
- `const std::vector<size_t>& getNeighbors() const` - Get neighbors
- `float getSlope() const` - Get cached slope
- `void calculateNormalFromFaces(const std::vector<glm::vec3>& faceNormals)` - Average normals
- `void clearNeighbors()` - Reset neighbors

**Private Methods:**
- `float calculateSlope() const` - Calculate slope with caching
- `void markSlopesDirty()` - Invalidate slope cache

---

## modules/planet/src/planet/vertexvisitor.h

### Class: `VertexVisitor` (abstract)
Visitor for vertex operations.

**Public Methods:**
- `virtual void visit(VertexData& vertex) = 0` - Visit vertex (pure virtual)

---

## modules/planet/src/planet/noiseterrainvisitor.h

### Class: `NoiseTerrainVisitor : VertexVisitor`
Apply noise-based terrain modifications.

**Public Methods:**
- `NoiseTerrainVisitor(float scale, float magnitude)` - Constructor
- `void visit(VertexData& vertex) override` - Apply noise to vertex

---

**End of Planet Module Index**
