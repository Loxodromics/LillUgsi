#include "planetgenerator.h"
#include "terraingeneratorvisitor.h"
#include "heightdifferencevisitor.h"

#include <spdlog/spdlog.h>

namespace lillugsi::planet {

PlanetGenerator::PlanetGenerator(
	std::shared_ptr<PlanetData> planetData, std::shared_ptr<rendering::IcosphereMesh>& mesh)
	: planetData(std::move(planetData))
	, mesh(mesh) {
	spdlog::debug("Created PlanetGenerator instance");
}

void PlanetGenerator::generateTerrain() const
{
	/// First, apply noise-based terrain generation to planet data
	/// This ensures our core data structure is updated first
	TerrainGeneratorVisitor visitor(this->settings);
	this->planetData->applyVertexVisitor(visitor);

	// HeightDifferenceVisitor visitor2(0.01);
	// this->planetData->applyVertexVisitor(visitor2);

	/// Update all normals
	this->planetData->updateNormals();

	/// Then update the mesh to reflect the changes
	/// A separate update step allows for future optimizations
	if (!this->updateMesh()) {
		spdlog::error("Failed to update mesh after terrain generation");
	}
}

void PlanetGenerator::setSettings(const GeneratorSettings& settings) {
	this->settings = settings;
	spdlog::debug("Updated generator settings: frequency={}, amplitude={}, octaves={}",
		settings.baseFrequency, settings.amplitude, settings.octaves);
}

const PlanetGenerator::GeneratorSettings& PlanetGenerator::getSettings() const {
	return this->settings;
}

bool PlanetGenerator::updateMesh() const {
	/// Get mesh pointer from weak_ptr
	auto meshPtr = this->mesh.lock();
	if (!meshPtr) {
		spdlog::error("Mesh reference expired");
		return false;
	}

	/// Get current vertex positions from mesh
	/// These will be on the unit sphere and properly distributed
	const auto positions = meshPtr->getVertexPositions();

	/// Prepare transforms for mesh update
	/// We create one transform per vertex to update position, normal, and color
	std::vector<rendering::IcosphereMesh::VertexTransform> transforms;
	transforms.reserve(positions.size());

	/// Process each vertex
	for (const auto& position : positions) {
		rendering::IcosphereMesh::VertexTransform transform{};

		transform.oldPosition = position;

		/// Calculate new position based on elevation
		const double elevation = this->planetData->getHeightAt(position);
		transform.position = position * static_cast<float>(1.0 + elevation * 0.15);

		/// Calculate normal as normalized position
		/// This creates smooth lighting across the sphere
		/// Could be improved by calculating actual surface normal
		glm::dvec3 normal = this->planetData->getNormalAt(position);
		glm::dvec3 interpolatedNormal = this->planetData->getInterpolatedNormalAt(position);
		glm::dvec3 normalizedPostion = glm::normalize(transform.position);
		transform.normal = this->planetData->getNormalAt(position);
		// transform.normal = glm::normalize(transform.position);

	// 	spdlog::debug("Normal comparison at position ({:.3f}, {:.3f}, {:.3f}):",
	// position.x, position.y, position.z);
	// 	spdlog::debug("\tNearest vertex normal:     ({:.3f}, {:.3f}, {:.3f})",
	// 		normal.x, normal.y, normal.z);
	// 	spdlog::debug("\tInterpolated normal:       ({:.3f}, {:.3f}, {:.3f})",
	// 		interpolatedNormal.x, interpolatedNormal.y, interpolatedNormal.z);
	// 	spdlog::debug("\tNormalized position:       ({:.3f}, {:.3f}, {:.3f})",
	// 		normalizedPostion.x, normalizedPostion.y, normalizedPostion.z);
	// 	spdlog::debug("\tDifference from position:  ({:.3f}, {:.3f}, {:.3f})",
	// 		normal.x - normalizedPostion.x,
	// 		normal.y - normalizedPostion.y,
	// 		normal.z - normalizedPostion.z);


		/// Set color based on elevation
		/// Simple gradient from dark (low) to light (high)
		const double normalizedElevation = (elevation + 1.0) * 0.5;
		transform.color = glm::dvec3(normalizedElevation);

		transforms.push_back(transform);
	}

	/// Apply all transforms to mesh
	try {
		meshPtr->applyVertexTransforms(transforms);
		spdlog::debug("Updated {} mesh vertices", transforms.size());
		return true;
	}
	catch (const std::exception& e) {
		spdlog::error("Failed to apply vertex transforms: {}", e.what());
		return false;
	}
}

} /// namespace lillugsi::planet