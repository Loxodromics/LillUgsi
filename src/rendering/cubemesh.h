#pragma once

#include "mesh.h"

namespace lillugsi::rendering {
/// CubeMesh class, derived from Mesh
/// This class represents a simple cube mesh
class CubeMesh : public Mesh {
public:
	/// Constructor
	/// Calls generateGeometry to create the cube mesh upon instantiation
	CubeMesh();

	/// Implementation of generateGeometry for a cube
	/// This function creates the vertices and indices for a cube
	void generateGeometry() override;
};

} /// namespace lillugsi::rendering