# Lill Ugsi: Vulkan Learning Render
Rather than following yet another Vulkan beginner's tutorial, I asked [Claude](https://claude.ai) to take me by the hand as we build a Vulkan renderer together. This project focuses on improving my graphics programming skills rather than creating a finished product. While primarily meant for my personal learning, the code is well-commented, so others might find it useful as well.

## Motivation
1. Learning: Gain hands-on experience with Vulkan and understand the principles of modern GPU programming.
2. Best Practices: Implement software engineering best practices in a complex, performance-critical application. This includes modular design, RAII (Resource Acquisition Is Initialization), and effective error handling.
3. Graphics Techniques: Progressively implement and understand various graphics techniques, from basic rendering to advanced effects like physically-based rendering and global illumination.
4. Performance Optimization: Learn to optimize graphics applications, understanding the intricacies of GPU utilization and memory management.
5. Cross-Platform Development: Create a renderer that works on multiple platforms (focusing on macOS and Linux), understanding the challenges of cross-platform graphics development.

## Key Features
- Vulkan-based rendering pipeline
- Modular, extensible architecture
- Progressive implementation of graphics techniques
- Focus on learning and best practices rather than rapid development

## Development Approach
Follow the principle: **"make it work, make it right, make it fast"** [See](https://wiki.c2.com/?MakeItWorkMakeItRightMakeItFast)

The project will start with basic Vulkan initialization and simple rendering, gradually adding complexity. Each stage will build upon the previous, ensuring a working renderer that continuously improves. The development process will emphasize understanding each component thoroughly before moving to more advanced topics. This project serves as both a learning tool for Vulkan and an exploration of software architecture in graphics programming. It aims to balance theoretical understanding with practical implementation, providing a solid foundation for further graphics programming endeavors.

## Tools and Libraries
- Vulkan SDK
- SDL3 for window management and input
- GLM for mathematics
- spdlog for logging
- CMake and Conan for build management and dependency handling

Based on your project structure and source code, I'll help you write a Getting Started section for your README. This should go under your "Key Features" section:

```markdown
## Getting Started

### Prerequisites
- CMake 3.24 or higher
- Vulkan SDK 1.3.216 or higher (download from [LunarG](https://vulkan.lunarg.com/))
- Modern C++ compiler with C++17 support
- SDL3
- Conan package manager

### Building

1. Clone the repository

2. Create a build directory:
```bash
mkdir build
cd build
```

3. Install dependencies using Conan:
```bash
conan install .. --output-folder=. --build=missing
```

4. Configure and build with CMake:
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
```

### Running
After successful compilation, you can run the application:
```bash
./LillUgsi
```

The application will open a window displaying a demo scene with multiple cubes and dynamic lighting.

### Controls
- Right Mouse Button + Mouse Move: Rotate camera
- WASD: Move camera forward/left/backward/right
- QE: Move camera down/up

### Common Issues
- Make sure to set `Vulkan_DIR` in CMakeLists.txt to your Vulkan SDK path
- If building fails, check that all dependencies are properly installed and your compiler supports C++17

## Coding Style Guidelines
- Use CamelCase for methods and variables, use PascalCase for classes (classes start with a capital letter, variables and methods don't)
- Use all lowercase letters for filenames without underscores or hyphens
- Follow more or less the Qt's coding style guidelines, but access all members (variables and methods) via this-> instead of prefixing them with m_
- Use "pointer-to-type" style
- Place implementation (.cpp) and header (.h) files next to each other in the src directory
- Use three slashes (///) for comments to differentiate from commented code
- Use tabs for indentation (4 spaces)
- Prioritize documenting the 'why' rather than the 'what' in comments. Comments explain the rationale behind each step. Comments are written saying 'we' instead of 'I' or passive voice
- Use descriptive names for classes, variables, and functions
- Use const for variables and methods where possible
- Write modern C++ code with [[nodiscard]] and such modern C++ features