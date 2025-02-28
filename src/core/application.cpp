#include "application.h"

#include <spdlog/spdlog.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

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
	/// Initialize time tracking with current time
	this->lastFrameTime = std::chrono::steady_clock::now();

	while (this->isRunning) {
		/// Update time first to provide accurate timing to all systems
		this->updateTime();

		/// Process input and game events
		this->handleEvents();

		/// Update game state
		this->update();

		/// Perform fixed time step updates
		this->fixedUpdate();

		/// Render the frame
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
			case SDL_EVENT_KEY_UP:
				/// Check for screenshot key (e.g., F12)
				if (event.key.key == SDLK_F12) {
					this->takeScreenshot();
				}
			case SDL_EVENT_KEY_DOWN:
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
		if (this->renderer->recreateSwapChain(static_cast<uint32_t>(w),
			static_cast<uint32_t>(h))) {
				/// TODO: Swapchain recreation successful
			} else {
				/// TODO: Handle recreation failure
			}
		this->framebufferResized = false;
		return;
	}

	/// Update renderer state before drawing
	/// This ensures all rendering state is current with game time
	this->renderer->update(this->gameTime.deltaTime);

	/// Perform actual frame rendering
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

void Application::updateTime() {
	/// Get current time for this frame
	auto currentTime = std::chrono::steady_clock::now();

	/// Calculate real (unscaled) delta time
	float realDeltaTime = std::chrono::duration<float>(
		currentTime - this->lastFrameTime).count();

	/// Store current time for next frame
	this->lastFrameTime = currentTime;

	/// Apply time scaling and pause state
	if (this->gameTime.isPaused) {
		this->gameTime.deltaTime = 0.0f;
	} else {
		/// Scale delta time by timeScale
		this->gameTime.deltaTime = realDeltaTime * this->gameTime.timeScale;

		/// Update total game time
		this->gameTime.totalTime += this->gameTime.deltaTime;

		/// We use integer division to check if we crossed a 5 second boundary
		int lastInterval = static_cast<int>((this->gameTime.totalTime - this->gameTime.deltaTime) / this->logInterval);
		int currentInterval = static_cast<int>(this->gameTime.totalTime / this->logInterval);

		if (currentInterval > lastInterval) {
			spdlog::info("Game time: {:.2f} seconds", this->gameTime.totalTime);
		}

		/// Accumulate time for fixed updates
		this->fixedTimeAccumulator += this->gameTime.deltaTime;
	}

	/// Cap delta time to prevent spiral of death
	/// If frame time is too high, we clamp it to maintain stability
	if (this->gameTime.deltaTime > this->maxDeltaTime) {
		spdlog::warn("Delta time capped from {} to {}",
			this->gameTime.deltaTime, this->maxDeltaTime);
		this->gameTime.deltaTime = this->maxDeltaTime;
	}
}

void Application::update() {
	/// Game logic update with variable time step
	/// Pass the scaled delta time to all systems
	if (this->renderer) {
		/// Update camera with scaled time
		this->renderer->getCamera()->update(this->gameTime.deltaTime);
	}
}

void Application::fixedUpdate() {
	/// Process all accumulated fixed updates
	/// This ensures simulation stability by using a fixed time step
	while (this->fixedTimeAccumulator >= GameTime::fixedTimeStep) {
		/// Perform fixed update step
		/// This is where physics and other time-critical updates should happen

		/// Subtract fixed time step from accumulator
		this->fixedTimeAccumulator -= GameTime::fixedTimeStep;
	}
}

void Application::takeScreenshot() const {
	const auto now = std::chrono::system_clock::now();
	const std::time_t current_time = std::chrono::system_clock::to_time_t(now);
	/// Convert to string using stringstream
	std::stringstream ss;
	ss << std::put_time(std::localtime(&current_time), "LillUgsi_%Y-%m-%d_%H.%M.%S.png");
	const std::string filename = ss.str();
	this->renderer->captureScreenshot(filename);
}

}
