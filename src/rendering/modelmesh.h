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
		spdlog::info("ModelMesh setGeometryData");
		this->markBuffersDirty();
	}
	
	/// This implementation just uses the data set via setGeometryData
	void generateGeometry() override {
		/// The geometry is already set via setGeometryData
		/// This method exists to satisfy the base class interface
	}
};

} // namespace lillugsi::rendering