#pragma once

#include "vertex.h"
#include <glm/glm.hpp>
#include <vector>

namespace lillugsi::rendering {

/// Utility class for calculating tangent vectors for normal mapping
/// This provides algorithms for generating tangent vectors based on 
/// vertex positions, normals, and texture coordinates
class TangentCalculator {
public:
	/// Calculate tangents for a triangle mesh
	/// This is the standard method for calculating tangents using triangle data
	/// It assumes the mesh uses triangles (3 indices per face)
	/// 
	/// @param vertices Vector of vertices to update with tangent vectors
	/// @param indices Vector of indices defining the triangles
	static void calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
		/// Early out if we don't have enough data
		/// We need at least one triangle (3 indices) to calculate tangents
		if (indices.size() < 3) {
			return;
		}
		
		/// First, initialize all tangents to zero
		/// We'll accumulate tangent contributions from each triangle
		/// and normalize at the end to get the average tangent
		for (auto& vertex : vertices) {
			vertex.tangent = glm::vec3(0.0f);
		}
		
		/// Process each triangle
		/// For each triangle, we calculate a tangent and contribute it to each vertex
		for (size_t i = 0; i < indices.size(); i += 3) {
			/// Get indices for this triangle
			uint32_t i0 = indices[i];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];
			
			/// Verify indices are in bounds to prevent access violations
			if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
				continue;
			}
			
			/// Get references to the vertices for more readable code
			Vertex& v0 = vertices[i0];
			Vertex& v1 = vertices[i1];
			Vertex& v2 = vertices[i2];
			
			/// Calculate triangle edges
			glm::vec3 edge1 = v1.position - v0.position;
			glm::vec3 edge2 = v2.position - v0.position;
			
			/// Calculate differences in texture coordinates
			glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
			glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;
			
			/// Calculate the tangent
			/// We use the formula derived from the relationship between
			/// position differences and texture coordinate differences
			float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
			
			glm::vec3 tangent;
			tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
			tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
			tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
			
			/// Handle degenerate cases where the determinant is zero or very small
			/// This happens when texture coordinates are collinear or nearly collinear
			if (!std::isfinite(f) || glm::length(tangent) < 0.00001f) {
				/// Fallback: use an arbitrary tangent perpendicular to the normal
				/// We use the cross product of the normal and a unit X or Y axis
				/// depending on which is more perpendicular to the normal
				glm::vec3 normal = (v0.normal + v1.normal + v2.normal) / 3.0f;
				if (std::abs(normal.x) > std::abs(normal.y)) {
					tangent = glm::normalize(glm::cross(normal, glm::vec3(0.0f, 1.0f, 0.0f)));
				} else {
					tangent = glm::normalize(glm::cross(normal, glm::vec3(1.0f, 0.0f, 0.0f)));
				}
			} else {
				/// Normalize the tangent
				tangent = glm::normalize(tangent);
			}
			
			/// Accumulate the tangent to each vertex of the triangle
			/// We'll average these contributions later
			v0.tangent += tangent;
			v1.tangent += tangent;
			v2.tangent += tangent;
		}
		
		/// Normalize all tangents
		/// This gives us the average tangent direction for each vertex
		for (auto& vertex : vertices) {
			/// Only normalize if the tangent has a non-zero length
			if (glm::length(vertex.tangent) > 0.00001f) {
				vertex.tangent = glm::normalize(vertex.tangent);
			} else {
				/// Fallback for vertices with zero accumulated tangent
				/// (could happen for isolated vertices not part of any triangle)
				if (std::abs(vertex.normal.x) > std::abs(vertex.normal.y)) {
					vertex.tangent = glm::normalize(glm::cross(vertex.normal, glm::vec3(0.0f, 1.0f, 0.0f)));
				} else {
					vertex.tangent = glm::normalize(glm::cross(vertex.normal, glm::vec3(1.0f, 0.0f, 0.0f)));
				}
			}
			
			/// Make sure the tangent is orthogonal to the normal
			/// This is important for creating an orthonormal TBN basis
			vertex.tangent = glm::normalize(
				vertex.tangent - vertex.normal * glm::dot(vertex.normal, vertex.tangent)
			);
		}
	}
	
	/// Calculate tangents for a quad-based mesh like a cube
	/// This is a specialized method for meshes where each face is a quad
	/// with well-defined texture coordinates
	/// 
	/// @param vertices Vector of vertices to update with tangent vectors
	/// @param indices Vector of indices defining the quads (4 indices per face)
	static void calculateTangentsForQuads(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
		/// For quads, we need to convert to triangles first
		/// Each quad (4 vertices) becomes 2 triangles (6 indices)
		std::vector<uint32_t> triangleIndices;
		triangleIndices.reserve(indices.size() * 6 / 4);
		
		for (size_t i = 0; i < indices.size(); i += 4) {
			/// First triangle: 0, 1, 2
			triangleIndices.push_back(indices[i]);
			triangleIndices.push_back(indices[i + 1]);
			triangleIndices.push_back(indices[i + 2]);
			
			/// Second triangle: 0, 2, 3
			triangleIndices.push_back(indices[i]);
			triangleIndices.push_back(indices[i + 2]);
			triangleIndices.push_back(indices[i + 3]);
		}
		
		/// Now use the standard triangle-based calculation
		calculateTangents(vertices, triangleIndices);
	}
};

} /// namespace lillugsi::rendering