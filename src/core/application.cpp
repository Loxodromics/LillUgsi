#include "application.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_vulkan.h>

namespace lillugsi::core {
Application::Application(const std::string& appName, uint32_t width, uint32_t height)
	: appName(appName)
	  , width(width)
	  , height(height)
	  , window(nullptr)
	  , isRunning(false) {
}

Application::~Application() {
	this->cleanup();
}

bool Application::initialize() {
	/// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) != true) {
		spdlog::error("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return false;
	}

	/// Create window
	this->window = SDL_CreateWindow(
		this->appName.c_str(),
		static_cast<int>(this->width),
		static_cast<int>(this->height),
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
	);

	if (this->window == nullptr) {
		spdlog::error("Window could not be created! SDL_Error: {}", SDL_GetError());
		return false;
	}

	/// Create and initialize the renderer
	this->renderer = std::make_unique<lillugsi::rendering::Renderer>();
	if (!this->renderer->initialize(this->window)) {
		spdlog::error("Failed to initialize the renderer");
		return false;
	}

	this->isRunning = true;
	this->framebufferResized = false;
	return true;
}

void Application::run() {
	while (this->isRunning) {
		this->handleEvents();
		this->render();
	}
}

void Application::handleEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				this->isRunning = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				this->framebufferResized = true;
				break;
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_MOUSE_MOTION:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				/// Handle camera input for events not handled by the main application
			/// This allows the camera to respond to mouse and keyboard input
				this->handleCameraInput(event);

				break;
			default:
				break;
		}
	}
}

void Application::render() {
	if (this->framebufferResized) {
		int w, h;
		SDL_GetWindowSizeInPixels(this->window, &w, &h);
		if (this->renderer->recreateSwapChain(static_cast<uint32_t>(w), static_cast<uint32_t>(h))) {
			// TODO: Swapchain recreation successful
		} else {
			// TODO:Handle recreation failure
		}
		this->framebufferResized = false;
		return;  // Skip this frame
	}
	this->renderer->drawFrame();
}

void Application::cleanup() {
	if (this->renderer) {
		this->renderer->cleanup();
	}

	if (this->window) {
		SDL_DestroyWindow(this->window);
		this->window = nullptr;
	}

	SDL_Quit();
}

void Application::handleCameraInput(const SDL_Event& event) {
	/// Delegate camera input handling to the renderer
/// This keeps the camera logic within the rendering system
	if (this->renderer) {
		this->renderer->handleCameraInput(this->window, event);
	}
}

}
