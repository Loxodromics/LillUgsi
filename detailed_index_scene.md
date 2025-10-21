# LillUgsi Scene Management - Detailed Index

This file contains detailed information about the scene graph and spatial management classes.

---

## src/scene/scenetypes.h

### Type: `NodeID`
64-bit unique scene node identifier.

### Enum: `BoundsType`
Bounding volume strategies.

**Values:**
- `Box`, `Sphere`, `OBB`

### Enum: `VisibilityStatus`
Culling/visibility state.

**Values:**
- `Visible`, `Culled`, `PartiallyVisible`

### Struct: `Transform`
Position, rotation, scale with matrix conversion.

**Members:**
- `glm::vec3 position`, `scale`
- `glm::quat rotation`

**Methods:**
- `glm::mat4 toMatrix() const` - Convert to matrix
- `static Transform fromMatrix(const glm::mat4&)` - Create from matrix

---

## src/scene/scenenode.h

### Class: `SceneNode`
Hierarchical scene graph node (shared_ptr, non-copyable).

**Public Methods:**
- `void addChild(std::shared_ptr<SceneNode> child)` - Add child node
- `bool removeChild(const std::shared_ptr<SceneNode>& child)` - Remove child
- `void setMesh(std::shared_ptr<rendering::Mesh> mesh)` - Associate mesh
- `void setLocalTransform(const Transform& transform)` - Set local transform
- `Transform getWorldTransform() const` - Get combined transform
- `Transform getLocalTransform() const` - Get local transform
- `std::weak_ptr<SceneNode> getParent() const` - Get parent
- `const std::vector<std::shared_ptr<SceneNode>>& getChildren() const` - Get children
- `std::shared_ptr<rendering::Mesh> getMesh() const` - Get mesh
- `scene::BoundingBox getWorldBounds() const` - Get world-space bounds
- `void updateWorldTransform()` - Propagate transform to hierarchy
- `bool isVisible(const Frustum& frustum) const` - Frustum culling test
- `void getRenderData(const Frustum& frustum, std::vector<rendering::Mesh::RenderData>& outRenderData) const` - Collect visible render data

---

## src/scene/frustum.h

### Struct: `Frustum::Plane`
Frustum plane with normal and distance.

**Members:**
- `glm::vec3 normal`
- `float distance`

**Methods:**
- `bool isInFront(const glm::vec3& point) const` - Point-plane test

### Class: `Frustum`
View frustum for culling.

**Public Static Methods:**
- `static Frustum createFromMatrix(const glm::mat4& viewProjection)` - Extract from VP matrix

**Public Methods:**
- `bool containsPoint(const glm::vec3& point) const` - Point-in-frustum test
- `bool intersectsBox(const BoundingBox& box) const` - Box intersection (primary culling)
- `std::array<glm::vec3, 8> getCorners() const` - Get frustum corners

---

## src/scene/boundingbox.h

### Class: `BoundingBox`
Axis-aligned bounding box.

**Public Methods:**
- `BoundingBox()` - Default constructor (invalid state)
- `void reset()` - Clear to invalid state
- `void addPoint(const glm::vec3& point)` - Expand to contain point
- `BoundingBox transform(const glm::mat4& matrix) const` - Transform box
- `bool intersects(const BoundingBox& other) const` - Box-box collision
- `bool contains(const glm::vec3& point) const` - Point-in-box test
- `std::array<glm::vec3, 8> getCorners() const` - Get 8 corners
- `glm::vec3 getCenter() const` - Get center point
- `glm::vec3 getSize() const` - Get dimensions
- `bool isValid() const` - Check validity
- `glm::vec3 getMin() const` - Get minimum corner
- `glm::vec3 getMax() const` - Get maximum corner

---

## src/scene/scene.h

### Class: `Scene` (non-copyable)
Top-level scene manager.

**Public Methods:**
- `std::shared_ptr<SceneNode> createNode(std::shared_ptr<SceneNode> parent = nullptr)` - Create node
- `bool removeNode(const std::shared_ptr<SceneNode>& node)` - Remove node and children
- `void update(float deltaTime)` - Update transforms and bounds
- `std::vector<rendering::Mesh::RenderData> getRenderData(const Frustum& frustum) const` - Get visible objects
- `std::shared_ptr<SceneNode> getRoot() const` - Get root node
- `size_t getNodeCount() const` - Get total node count
- `void setTerrainRoot(std::shared_ptr<SceneNode> terrainRoot)` - Set terrain node
- `std::shared_ptr<SceneNode> getTerrainRoot() const` - Get terrain node
- `void forEachMesh(const std::function<void(rendering::Mesh*)>& func)` - Apply function to all meshes

---
