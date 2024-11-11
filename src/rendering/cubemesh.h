#pragma once

#include "mesh.h"

namespace lillugsi::rendering {
/// CubeMesh class, derived from Mesh
/// This class represents a simple cube mesh

class CubeMesh : public Mesh {
public:
	CubeMesh(float sideLength = 1.0f);
	virtual ~CubeMesh() = default;

	virtual void generateGeometry() override;

	/// Set colors for each face of the cube
	void setFaceColors(const std::vector<glm::vec3>& colors);

private:
	float sideLength;
	std::vector<glm::vec3> faceColors;

	/// Define the indices for each face of the cube
	static const std::array<std::array<int, 4>, 6> CubeFaceIndices;

	/// Default colors for the cube faces
	static const std::array<glm::vec3, 6> DefaultColors;

	/// Helper function to get face normal
	glm::vec3 getFaceNormal(int face) const {
		switch(face) {
		case 0: return glm::vec3( 0.0f,  0.0f, -1.0f); /// Front
		case 1: return glm::vec3( 0.0f,  0.0f,  1.0f); /// Back
		case 2: return glm::vec3( 1.0f,  0.0f,  0.0f); /// Right
		case 3: return glm::vec3(-1.0f,  0.0f,  0.0f); /// Left
		case 4: return glm::vec3( 0.0f,  1.0f,  0.0f); /// Top
		case 5: return glm::vec3( 0.0f, -1.0f,  0.0f); /// Bottom
		default: return glm::vec3(0.0f);
		}
	}
};

} /// namespace lillugsi::rendering