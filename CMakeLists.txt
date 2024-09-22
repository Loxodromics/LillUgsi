cmake_minimum_required(VERSION 3.24)
project(LillUgsi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the path to your Vulkan SDK
set(Vulkan_DIR "/Users/philipp/VulkanSDK/1.3.216.0/macOS")

find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

add_executable(LillUgsi
		src/main.cpp
#		src/core/application.cpp
		src/vulkan/VulkanHandle.h
		src/vulkan/vulkaninstance.h
		src/vulkan/vulkaninstance.cpp
)

target_include_directories(LillUgsi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${Vulkan_INCLUDE_DIRS})

target_link_libraries(LillUgsi
		PRIVATE
		SDL3::SDL3
		spdlog::spdlog
		Vulkan::Vulkan
)

# Shader compilation (you'll need to implement this later)
# add_custom_target(Shaders
#     COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
#     COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Simple.vert -o ${CMAKE_BINARY_DIR}/shaders/Simple.vert.spv
#     COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Simple.frag -o ${CMAKE_BINARY_DIR}/shaders/Simple.frag.spv
# )
# add_dependencies(LillUgsi Shaders)