#pragma once
#include "planetdata.h"
#include "face.h"
#include <glm/glm.hpp>


namespace lillugsi::planet {

class PlanetGenerator {
public:
	struct GeneratorSettings {
		float baseFrequency{0.1f};
		float amplitude{1.0f};
		uint32_t octaves{4};
		float persistence{0.5f};
		float lacunarity{2.0f};
	};

	explicit PlanetGenerator(std::shared_ptr<PlanetData> planetData);

	/// Generate terrain using current settings
	void generateTerrain() const;

	/// Set generator settings
	void setSettings(const GeneratorSettings& settings);

	/// Get current settings
	[[nodiscard]] const GeneratorSettings& getSettings() const;

private:
	std::shared_ptr<PlanetData> planetData;
	GeneratorSettings settings;
};

} /// namespace lillugsi::planet