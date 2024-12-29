#pragma once

#include "vertex.h"
#include "material.h"
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <vector>
#include <memory>

namespace lillugsi::vulkan {
class VertexBuffer;
class IndexBuffer;
}


namespace lillugsi::rendering {

class Mesh {
public:
	/// Data structure containing everything needed to render this mesh
	/// This structure is populated by prepareRenderData and used by the renderer
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
	virtual void generateGeometry() = 0;

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
		this->material = material;
	}

	/// Get the current material
	/// @return Shared pointer to the current material, or nullptr if none set
	[[nodiscard]] std::shared_ptr<Material> getMaterial() const {
		return this->material;
	}

protected:
	/// Vertex data stored in CPU memory
	std::vector<Vertex> vertices;

	/// Index data stored in CPU memory
	std::vector<uint32_t> indices;

	/// Translation vector for positioning the mesh
	glm::vec3 translation{0.0f};

	/// GPU buffers
	std::shared_ptr<vulkan::VertexBuffer> vertexBuffer;
	std::shared_ptr<vulkan::IndexBuffer> indexBuffer;

	/// We use a shared_ptr to share materials between meshes and ensure proper lifecycle management
	std::shared_ptr<Material> material;
};

} /// namespace lillugsi::rendering