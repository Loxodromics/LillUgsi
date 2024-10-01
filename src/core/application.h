#pragma once

#include "rendering/renderer.h"
#include <SDL3/SDL.h>
#include <string>
#include <memory>

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

private:
	/// Handle input events
	/// This method processes SDL events and updates the application state accordingly
	void handleEvents();

	/// Handle camera-specific input
	/// This method delegates camera input to the renderer
	/// @param event The SDL event to process
	void handleCameraInput(const SDL_Event& event);

	/// Perform rendering
	void render();

	std::string appName;
	uint32_t width;
	uint32_t height;

	SDL_Window* window;
	std::unique_ptr<Renderer> renderer;

	bool isRunning;
	bool framebufferResized;
};