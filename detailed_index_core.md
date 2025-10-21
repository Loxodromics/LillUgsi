# LillUgsi Core Application - Detailed Index

This file contains detailed information about the core application layer classes and functions.

---

## src/core/application.h

### Struct: `GameTime`
Time management structure separating game time from real time.

**Members:**
- `float deltaTime` - Time elapsed since last frame
- `float totalTime` - Total running time
- `float timeScale` - Scale factor for time (1.0 = normal)
- `bool isPaused` - Pause state
- `static constexpr float fixedTimeStep` - Fixed time step for physics (1/60)

### Class: `Application`
Main application class managing game loop and window lifecycle.

**Public Methods:**
- `Application(const std::string& appName, uint32_t width, uint32_t height)` - Constructor with window configuration
- `~Application()` - Destructor
- `bool initialize()` - Initialize the application
- `void run()` - Run the main game loop
- `void cleanup()` - Clean up resources
- `const GameTime& getGameTime() const` - Get current game time information
- `void setTimeLogInterval(float interval)` - Set time logging interval
- `void setMaxDeltaTime(float maxDelta)` - Set maximum allowed delta time

**Protected Methods:**
- `void handleEvents()` - Process SDL events and update application state
- `void handleCameraInput(const SDL_Event& event)` - Delegate camera input to renderer
- `void update()` - Update game state with delta time
- `void fixedUpdate()` - Perform fixed time step updates
- `void render()` - Perform rendering
- `void updateTime()` - Manage frame timing, scaling, and fixed time step
- `void takeScreenshot() const` - Capture and save screenshot

---

## src/main.cpp

### Function: `main(int argc, char* argv[])`
Entry point for the LillUgsi application.

**Responsibilities:**
- Initialize spdlog logging
- Create and initialize Application instance
- Run main loop
- Handle exceptions and return appropriate exit codes
