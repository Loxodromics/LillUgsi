#pragma once

#include "mesh.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

/// ModelMesh extends Mesh to support direct vertex and index data assignment
/// This is used for meshes loaded from model files rather than procedurally generated
class ModelMesh : public Mesh {
public:
	ModelMesh() = default;
	~ModelMesh() override = default;

	/// Set the mesh geometry data directly
	/// @param vertices Vector of vertex data
	/// @param indices Vector of index data
	void setGeometryData(std::vector<Vertex> vertices, std::vector<uint32_t> indices) {
		this->vertices = std::move(vertices);
		this->indices = std::move(indices);
		spdlog::debug("ModelMesh geometry set: {} vertices, {} indices",
		    this->vertices.size(), this->indices.size());
		this->markBuffersDirty();
	}

	/// For ModelMesh, this implementation just validates the pre-set data
	void generateGeometry() override {
		/// If we have no data yet, return early
		if (this->vertices.empty() || this->indices.empty()) {
			spdlog::debug("ModelMesh has no geometry data");
			return;
		}

		spdlog::debug("ModelMesh already has geometry: {} vertices, {} indices",
		    this->vertices.size(), this->indices.size());
	}
};

} /// namespace lillugsi::rendering