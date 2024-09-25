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
	/// Handle SDL events
	void handleEvents();

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