#include "cubemesh.h"
#include <array>

namespace lillugsi::rendering {

CubeMesh::CubeMesh(float sideLength) : sideLength(sideLength) {
	this->faceColors = std::vector<glm::vec3>(DefaultColors.begin(), DefaultColors.end());
	this->generateGeometry();
}

void CubeMesh::generateGeometry() {
	/// Clear any existing data
	this->vertices.clear();
	this->indices.clear();

	/// Half of the side length, used to calculate vertex positions
	float halfSide = this->sideLength / 2.0f;

	/// Define the 8 vertices of the cube
	std::array<glm::vec3, 8> positions = {
		glm::vec3(-halfSide, -halfSide, -halfSide),
		glm::vec3( halfSide, -halfSide, -halfSide),
		glm::vec3( halfSide,  halfSide, -halfSide),
		glm::vec3(-halfSide,  halfSide, -halfSide),
		glm::vec3(-halfSide, -halfSide,  halfSide),
		glm::vec3( halfSide, -halfSide,  halfSide),
		glm::vec3( halfSide,  halfSide,  halfSide),
		glm::vec3(-halfSide,  halfSide,  halfSide)
	};

	/// Define the normals for each face
	std::array<glm::vec3, 6> normals = {
		glm::vec3( 0.0f,  0.0f, -1.0f), /// Front
		glm::vec3( 0.0f,  0.0f,  1.0f), /// Back
		glm::vec3( 1.0f,  0.0f,  0.0f), /// Right
		glm::vec3(-1.0f,  0.0f,  0.0f), /// Left
		glm::vec3( 0.0f,  1.0f,  0.0f), /// Top
		glm::vec3( 0.0f, -1.0f,  0.0f)  /// Bottom
	};

	/// Generate vertices for each face
	for (int face = 0; face < 6; ++face) {
		for (int i = 0; i < 4; ++i) {
			Vertex vertex;
			vertex.position = positions[CubeFaceIndices[face][i]];
			vertex.normal = normals[face];
			vertex.color = this->faceColors[face];
			this->vertices.push_back(vertex);
		}

		/// Generate indices for this face
		uint32_t baseIndex = face * 4;
		this->indices.push_back(baseIndex);
		this->indices.push_back(baseIndex + 1);
		this->indices.push_back(baseIndex + 2);
		this->indices.push_back(baseIndex);
		this->indices.push_back(baseIndex + 2);
		this->indices.push_back(baseIndex + 3);
	}
}

void CubeMesh::setFaceColors(const std::vector<glm::vec3>& colors) {
	if (colors.size() == 6) {
		this->faceColors = colors;
		this->generateGeometry();
	}
}

/// Define the indices for each face of the cube
const std::array<std::array<int, 4>, 6> CubeMesh::CubeFaceIndices = {{
	{0, 1, 2, 3}, /// Front
	{5, 4, 7, 6}, /// Back
	{1, 5, 6, 2}, /// Right
	{4, 0, 3, 7}, /// Left
	{3, 2, 6, 7}, /// Top
	{4, 5, 1, 0}  /// Bottom
}};

/// Define default colors for the cube faces
const std::array<glm::vec3, 6> CubeMesh::DefaultColors = {
	glm::vec3(1.0f, 0.0f, 0.0f), /// Red
	glm::vec3(0.0f, 1.0f, 0.0f), /// Green
	glm::vec3(0.0f, 0.0f, 1.0f), /// Blue
	glm::vec3(1.0f, 1.0f, 0.0f), /// Yellow
	glm::vec3(1.0f, 0.0f, 1.0f), /// Magenta
	glm::vec3(0.0f, 1.0f, 1.0f)  /// Cyan
};

} /// namespace lillugsi::rendering
