#pragma once

#include "material.h"
#include "vertex.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

namespace lillugsi::vulkan {
class VertexBuffer;
class IndexBuffer;
}


namespace lillugsi::rendering {

class Mesh {
friend class ModelLoader;
friend class GltfModelLoader;

public:
	/// Data structure containing everything needed to render this mesh
	/// This struct is populated by prepareRenderData and used by the renderer
	struct RenderData {
		/// Transform data
		glm::mat4 modelMatrix{1.0f};

		/// Buffer handles
		std::shared_ptr<vulkan::VertexBuffer> vertexBuffer;
		std::shared_ptr<vulkan::IndexBuffer> indexBuffer;

		/// Material data for rendering appearance
		/// We use a shared_ptr to ensure the material stays alive
		/// as long as any RenderData referring to it exists
		std::shared_ptr<Material> material;

		/// Future expansion fields:
		/// bool isTransparent;     /// For render sorting
		/// float distanceToCamera; /// For LOD/culling
	};

	Mesh() = default;
	virtual ~Mesh() = default;

	/// Generate mesh geometry
	/// This pure virtual function must be implemented by derived classes
	/// to define their specific geometry
	/// Default implementation does nothing
	virtual void generateGeometry() {};

	/// Prepare render data for this mesh
	/// This method populates a RenderData struct with everything needed to render the mesh
	/// @param data Reference to RenderData struct to populate
	virtual void prepareRenderData(RenderData& data) const {
		data.modelMatrix = glm::translate(glm::mat4(1.0f), this->translation);
		data.vertexBuffer = this->vertexBuffer;
		data.indexBuffer = this->indexBuffer;
		data.material = this->material;
	}

	/// Get vertex data (used during buffer creation)
	/// @return A const reference to the vector of vertices
	const std::vector<Vertex>& getVertices() const { return this->vertices; }

	/// Get index data (used during buffer creation)
	/// @return A const reference to the vector of indices
	const std::vector<uint32_t>& getIndices() const { return this->indices; }

	/// Set the mesh's GPU buffers
	/// This is called by MeshManager after creating the buffers
	/// @param vBuffer Vertex buffer for this mesh
	/// @param iBuffer Index buffer for this mesh
	void setBuffers(std::shared_ptr<vulkan::VertexBuffer> vBuffer,
		std::shared_ptr<vulkan::IndexBuffer> iBuffer) {
		this->vertexBuffer = std::move(vBuffer);
		this->indexBuffer = std::move(iBuffer);
	}

	/// Set the translation of the mesh
	void setTranslation(const glm::vec3& translation) {
		this->translation = translation;
		this->generateGeometry();
	}

	/// Set the material for this mesh
	/// The material defines how the mesh is rendered
	/// @param material Shared pointer to the material to use
	void setMaterial(std::shared_ptr<Material> material) {
		this->material = std::move(material);
	}

	/// Get the current material
	/// @return Shared pointer to the current material, or nullptr if none set
	[[nodiscard]] std::shared_ptr<Material> getMaterial() const {
		return this->material;
	}

	/// Mark mesh buffers as needing update
	/// This signals to the rendering system that GPU buffers need to be rebuilt
	void markBuffersDirty() {
		this->buffersDirty = true;
		spdlog::trace("Marked buffers dirty for mesh");
	}

	/// Check if buffers need updating
	/// @return true if buffers need to be rebuilt
	[[nodiscard]] bool needsBufferUpdate() const {
		return this->buffersDirty;
	}

	/// Reset dirty flag after buffer update
	/// Called by buffer management system after update is complete
	void clearBuffersDirty() {
		this->buffersDirty = false;
		spdlog::trace("Cleared buffers dirty flag for mesh");
	}

	/// Set texture tiling factor for this mesh
	/// This controls how many times textures repeat across the mesh
	/// Higher values create more texture repetitions
	/// @param uTiling Horizontal tiling factor
	/// @param vTiling Vertical tiling factor
	void setTextureTiling(float uTiling, float vTiling) {
		this->textureTilingU = uTiling;
		this->textureTilingV = vTiling;

		/// Regenerate geometry with new UV scaling
		if (this->textureTilingU > 0.0f && this->textureTilingV > 0.0f) {
			this->markBuffersDirty();
			this->generateGeometry();
		}
	}

	/// Get the horizontal texture tiling factor
	/// @return The U tiling factor
	[[nodiscard]] float getTextureTilingU() const { return this->textureTilingU; }

	/// Get the vertical texture tiling factor
	/// @return The V tiling factor
	[[nodiscard]] float getTextureTilingV() const { return this->textureTilingV; }

protected:
	/// Apply texture tiling to a UV coordinate
	/// This helper method ensures consistent tiling across mesh implementations
	/// @param uv The original UV coordinate to scale
	/// @return The scaled UV coordinate based on current tiling factors
	[[nodiscard]] glm::vec2 applyTextureTiling(const glm::vec2& uv) const {
		return glm::vec2(uv.x * this->textureTilingU, uv.y * this->textureTilingV);
	}

	/// Vertex data stored in CPU memory
	std::vector<Vertex> vertices;

	/// Index data stored in CPU memory
	std::vector<uint32_t> indices;

	/// Translation vector for positioning the mesh
	glm::vec3 translation{0.0f};

	/// GPU buffers
	std::shared_ptr<vulkan::VertexBuffer> vertexBuffer;
	std::shared_ptr<vulkan::IndexBuffer> indexBuffer;

	/// Flag indicating if GPU buffers need updating
	/// Set when vertex data changes, cleared after buffer rebuild
	bool buffersDirty{false};

	/// We use a shared_ptr to share materials between meshes and ensure proper lifecycle management
	std::shared_ptr<Material> material;

	/// Texture tiling factors control how textures are repeated across the mesh
	/// Higher values create more repetitions, useful for detailed surfaces
	/// Default value of 1.0 means no tiling (texture covers the mesh once)
	float textureTilingU{1.0f};
	float textureTilingV{1.0f};
};

} /// namespace lillugsi::rendering