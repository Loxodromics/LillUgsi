#pragma once

#include "mesh.h"
#include <glm/glm.hpp>
#include <tuple>

namespace lillugsi::rendering {

/// Torus knot mesh with parameterized winding numbers
/// Creates complex geometry with varying curvature for testing advanced lighting and AO
class TorusKnotMesh : public Mesh {
public:
	/// Construct a torus knot with specified parameters
	/// @param majorRadius Distance from origin to knot path center
	/// @param minorRadius Radius of the tube around the knot path
	/// @param p Number of toroidal windings
	/// @param q Number of poloidal windings
	/// @param segments Number of segments along the knot path
	/// @param tubeSegments Number of segments around the tube
	explicit TorusKnotMesh(
		float majorRadius = 1.0f,
		float minorRadius = 0.3f,
		uint32_t p = 2,
		uint32_t q = 3,
		uint32_t segments = 256,
		uint32_t tubeSegments = 24
	);

	void generateGeometry() override;

	/// Get the major radius
	[[nodiscard]] float getMajorRadius() const { return this->majorRadius; }

	/// Get the minor radius (tube radius)
	[[nodiscard]] float getMinorRadius() const { return this->minorRadius; }

	/// Get the p winding number (toroidal)
	[[nodiscard]] uint32_t getP() const { return this->p; }

	/// Get the q winding number (poloidal)
	[[nodiscard]] uint32_t getQ() const { return this->q; }

private:
	float majorRadius;
	float minorRadius;
	uint32_t p;
	uint32_t q;
	uint32_t segments;
	uint32_t tubeSegments;

	/// Compute position on the knot path
	/// @param t Parameter along knot [0, 2π]
	[[nodiscard]] glm::vec3 computeKnotPosition(float t) const;

	/// Compute tangent vector using finite differences
	/// @param t Parameter along knot [0, 2π]
	[[nodiscard]] glm::vec3 computeTangent(float t) const;

	/// Compute Frenet-Serret frame at parameter t
	/// @param t Parameter along knot [0, 2π]
	/// @param epsilon Step size for finite differences
	/// @return Tuple of {tangent, normal, binormal}
	[[nodiscard]] std::tuple<glm::vec3, glm::vec3, glm::vec3>
		computeFrenetFrame(float t, float epsilon = 0.001f) const;

	/// Compute face-averaged normals for all vertices
	void computeAveragedNormals();
};

} /// namespace lillugsi::rendering
