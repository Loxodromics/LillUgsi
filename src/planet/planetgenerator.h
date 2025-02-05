#pragma once

#include "planetdata.h"
#include "face.h"
#include "rendering/icospheremesh.h"

#include <glm/glm.hpp>

namespace lillugsi::planet {

class PlanetGenerator {
public:
	struct GeneratorSettings {
		float baseFrequency{2.0f};
		float amplitude{1.0f};
		uint32_t octaves{4};
		float persistence{0.5f};
		float lacunarity{2.0f};
		int seed{54321};
	};

	/// Create generator with planet data and mesh to modify
	/// @param planetData The planet data to generate terrain for
	/// @param mesh The mesh to visualize the terrain
	PlanetGenerator(
		std::shared_ptr<PlanetData> planetData,
		std::shared_ptr<rendering::IcosphereMesh>& mesh);

	/// Generate terrain and update mesh
	void generateTerrain() const;

	/// Modify terrain at a specific point
	/// @param position The point to modify (will be normalized)
	/// @param amount Amount of elevation change
	void modifyTerrain(const glm::dvec3& position, float amount);

	/// Update settings and regenerate terrain
	/// @param settings New generator settings
	void setSettings(const GeneratorSettings& settings);

	/// Get current settings
	/// @return Current generator settings
	[[nodiscard]] const GeneratorSettings& getSettings() const;

private:
	std::shared_ptr<PlanetData> planetData;
	std::weak_ptr<rendering::IcosphereMesh> mesh;
	GeneratorSettings settings;

	/// Update mesh vertices from current planet data
	/// @return true if mesh was updated successfully
	bool updateMesh() const;
};

} /// namespace lillugsi::planet