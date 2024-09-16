cmake_minimum_required(VERSION 3.15)
project(LillUgsi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#conan_basic_setup(TARGETS)

#find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

add_executable(LillUgsi
		src/main.cpp
		src/Application.cpp
		src/Core/Logger.cpp
		src/Renderer/VulkanRenderer.cpp
		src/Renderer/VulkanInitializer.cpp
		src/Window/WindowManager.cpp
		src/Event/EventSystem.cpp
)

target_include_directories(LillUgsi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(LillUgsi
		PRIVATE
		SDL2::SDL2
#		Vulkan::Vulkan
)

# Shader compilation (you'll need to implement this later)
# add_custom_target(Shaders
#     COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
#     COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Simple.vert -o ${CMAKE_BINARY_DIR}/shaders/Simple.vert.spv
#     COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Simple.frag -o ${CMAKE_BINARY_DIR}/shaders/Simple.frag.spv
# )
# add_dependencies(LillUgsi Shaders)