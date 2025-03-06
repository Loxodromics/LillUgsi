#include "cubemesh.h"

#include <array>
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

CubeMesh::CubeMesh(float sideLength)
	: sideLength(sideLength)
	, faceColors(DefaultColors.begin(), DefaultColors.end()) {
	spdlog::debug("Creating cube mesh with side length {}", sideLength);
}

void CubeMesh::generateGeometry() {
	spdlog::debug("Generating cube mesh geometry");

	/// Clear any existing data
	this->vertices.clear();
	this->indices.clear();

	/// Half of the side length, used to calculate vertex positions
	float halfSide = this->sideLength / 2.0f;

	/// Define the 8 vertices of the cube
	std::array<glm::vec3, 8> positions = {
		glm::vec3(-halfSide, -halfSide, -halfSide),  /// 0: left  bottom back
		glm::vec3( halfSide, -halfSide, -halfSide),  /// 1: right bottom back
		glm::vec3( halfSide,  halfSide, -halfSide),  /// 2: right top    back
		glm::vec3(-halfSide,  halfSide, -halfSide),  /// 3: left  top    back
		glm::vec3(-halfSide, -halfSide,  halfSide),  /// 4: left  bottom front
		glm::vec3( halfSide, -halfSide,  halfSide),  /// 5: right bottom front
		glm::vec3( halfSide,  halfSide,  halfSide),  /// 6: right top    front
		glm::vec3(-halfSide,  halfSide,  halfSide)   /// 7: left  top    front
	};

	spdlog::debug("Generating cube vertices and indices");

	/// Generate vertices for each face
	for (int face = 0; face < 6; ++face) {
		for (int i = 0; i < 4; ++i) {
			Vertex vertex;
			vertex.position = positions[CubeFaceIndices[face][i]];
			vertex.normal = getFaceNormal(face);
			vertex.color = this->faceColors[face];

			/// Apply texture coordinates for this vertex
			/// We use the DefaultUVs array which defines a standard mapping
			/// and apply the tiling factor to create texture repetition if needed
			vertex.texCoord = this->applyTextureTiling(DefaultUVs[i]);

			this->vertices.push_back(vertex);
		}

		/// Generate indices for this face (2 triangles)
		uint32_t baseIndex = face * 4;
		this->indices.push_back(baseIndex);
		this->indices.push_back(baseIndex + 1);
		this->indices.push_back(baseIndex + 2);
		this->indices.push_back(baseIndex);
		this->indices.push_back(baseIndex + 2);
		this->indices.push_back(baseIndex + 3);
	}

	spdlog::debug("Cube mesh generated with {} vertices and {} indices",
		this->vertices.size(), this->indices.size());

	/// Mark buffers as dirty to ensure they're updated when next rendered
	this->markBuffersDirty();
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

/// Define default UV coordinates for each vertex
/// These provide a standard mapping from texture to cube face
const std::array<glm::vec2, 4> CubeMesh::DefaultUVs = {
	glm::vec2(0.0f, 0.0f), /// Bottom-left
	glm::vec2(1.0f, 0.0f), /// Bottom-right
	glm::vec2(1.0f, 1.0f), /// Top-right
	glm::vec2(0.0f, 1.0f)  /// Top-left
};

} /// namespace lillugsi::rendering