#pragma once

#include "rendering/renderer.h"
#include <SDL3/SDL.h>
#include <string>
#include <memory>

namespace lillugsi::core {

/// Time management structure to track frame and game time
/// This separates game time from real time, allowing for
/// pausing, time scaling, and fixed time steps
struct GameTime {
	float deltaTime{0.0f};        /// Time elapsed since last frame
	float totalTime{0.0f};        /// Total running time
	float timeScale{1.0f};        /// Scale factor for time (1.0 = normal)
	bool isPaused{false};         /// Pause state

	/// Fixed time step for physics/simulation
	static constexpr float fixedTimeStep = 1.0f / 60.0f;
};

class Application {
public:
	Application(const std::string& appName, uint32_t width, uint32_t height);
	~Application();

	/// Initialize the application
	bool initialize();

	/// Run the main loop
	void run();

	/// Clean up resources
	void cleanup();

	/// Get the current game time information
	/// @return Reference to the current game time state
	const GameTime& getGameTime() const { return this->gameTime; }

	/// Set the interval for time logging
	/// @param interval Time in seconds between logs
	void setTimeLogInterval(float interval) { this->logInterval = interval; }

	/// Set the maximum allowed delta time
	/// @param maxDelta Maximum time step in seconds
	void setMaxDeltaTime(float maxDelta) { this->maxDeltaTime = maxDelta; }

protected:
	/// Handle input events
	/// This method processes SDL events and updates the application state accordingly
	void handleEvents();

	/// Handle camera-specific input
	/// This method delegates camera input to the renderer
	/// @param event The SDL event to process
	void handleCameraInput(const SDL_Event& event);

	/// Update game state
	/// @param deltaTime Time elapsed since last frame
	void update();

	/// Perform fixed time step updates
	/// @param deltaTime Time elapsed since last frame
	void fixedUpdate();

	/// Perform rendering
	void render();

	/// Update time tracking
	/// Manages frame timing, scaling, and fixed time step accumulation
	void updateTime();

	/// Take a screenshot and save it to a file with the current date
	void takeScreenshot() const;

	std::string appName;
	uint32_t width;
	uint32_t height;

	SDL_Window* window;
	std::unique_ptr<lillugsi::rendering::Renderer> renderer;

	bool isRunning;
	bool framebufferResized;

	/// Time management
	GameTime gameTime;
	std::chrono::steady_clock::time_point lastFrameTime;
	float fixedTimeAccumulator{0.0f}; /// Tracks leftover time for fixed updates
	float logInterval{5.0f};
	float maxDeltaTime{0.1f};
};
}
