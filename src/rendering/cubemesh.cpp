#include "cubemesh.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering
{
CubeMesh::CubeMesh() {
	this->generateGeometry();
}

void CubeMesh::generateGeometry() {
	/// Clear existing data to ensure we're starting fresh
	this->vertices.clear();
	this->indices.clear();

	/// Define the 8 vertices of a cube
	/// We're creating a cube centered at the origin with side length 1
	glm::vec3 v0(-0.5f, -0.5f, -0.5f);
	glm::vec3 v1(0.5f, -0.5f, -0.5f);
	glm::vec3 v2(0.5f,  0.5f, -0.5f);
	glm::vec3 v3(-0.5f,  0.5f, -0.5f);
	glm::vec3 v4(-0.5f, -0.5f,  0.5f);
	glm::vec3 v5(0.5f, -0.5f,  0.5f);
	glm::vec3 v6(0.5f,  0.5f,  0.5f);
	glm::vec3 v7(-0.5f,  0.5f,  0.5f);

	/// Define the vertices with positions, normals, and colors
	/// We're using a simple color scheme here, but you could modify this for more complex texturing or coloring
	this->vertices = {
		/// Front face (z = -0.5)
		{v0, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}}, /// 0
		{v1, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}}, /// 1
		{v2, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}}, /// 2
		{v3, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 0.0f}}, /// 3

		/// Back face (z = 0.5)
		{v4, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}},  /// 4
		{v5, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}},  /// 5
		{v6, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},  /// 6
		{v7, {0.0f, 0.0f, 1.0f}, {0.5f, 0.5f, 0.5f}},  /// 7

		/// Right face (x = 0.5)
		{v1, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},  /// 8
		{v5, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},  /// 9
		{v6, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},  /// 10
		{v2, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},  /// 11

		/// Left face (x = -0.5)
		{v4, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}}, /// 12
		{v0, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}, /// 13
		{v3, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}, /// 14
		{v7, {-1.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}}, /// 15

		/// Top face (y = 0.5)
		{v3, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},  /// 16
		{v2, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},  /// 17
		{v6, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},  /// 18
		{v7, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f, 0.5f}},  /// 19

		/// Bottom face (y = -0.5)
		{v4, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}}, /// 20
		{v5, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}}, /// 21
		{v1, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, /// 22
		{v0, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}  /// 23
	};

	/// Define the indices for the cube
	/// Each face is made up of two triangles
	this->indices = {
		0, 1, 2, 2, 3, 0,       /// Front face
		4, 5, 6, 6, 7, 4,       /// Back face
		8, 9, 10, 10, 11, 8,    /// Right face
		12, 13, 14, 14, 15, 12, /// Left face
		16, 17, 18, 18, 19, 16, /// Top face
		20, 21, 22, 22, 23, 20  /// Bottom face
	};

	spdlog::info("Cube mesh generated with {} vertices and {} indices", this->vertices.size(), this->indices.size());

	if (this->vertices.empty() || this->indices.empty()) {
		spdlog::error("CubeMesh generated empty geometry!");
	}
}
} /// namespace lillugsi::rendering