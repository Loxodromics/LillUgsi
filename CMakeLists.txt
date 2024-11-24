cmake_minimum_required(VERSION 3.24)
project(LillUgsi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_NDEBUG "Enable NDEBUG macro" OFF)

if(ENABLE_NDEBUG)
	add_compile_definitions(NDEBUG)
endif()

# Set the path to your Vulkan SDK
set(Vulkan_DIR "/Users/philipp/VulkanSDK/1.3.216.0/macOS")

find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

add_executable(LillUgsi
		src/main.cpp
		src/vulkan/vulkaninstance.cpp
		src/core/application.cpp
		src/vulkan/vulkandevice.cpp
		src/vulkan/vulkanswapchain.cpp
		src/rendering/renderer.cpp
		src/vulkan/vulkanbuffer.cpp
		src/rendering/camera.cpp
		src/rendering/editorcamera.cpp
		src/vulkan/vulkancontext.cpp
		src/vulkan/pipelinemanager.cpp
		src/rendering/cubemesh.cpp
		src/rendering/meshmanager.cpp
		src/vulkan/depthbuffer.cpp
		src/vulkan/shadermodule.cpp
		src/vulkan/shaderprogram.cpp
		src/rendering/buffercache.cpp
		src/scene/scenetypes.cpp
		src/scene/boundingbox.cpp
		src/scene/frustum.cpp
)

target_include_directories(LillUgsi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${Vulkan_INCLUDE_DIRS})

target_link_libraries(LillUgsi
		PRIVATE
		SDL3::SDL3
		spdlog::spdlog
		Vulkan::Vulkan
		glm::glm
)

add_custom_target(Shaders
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vert.glsl.vert -o ${CMAKE_BINARY_DIR}/shaders/vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frag.glsl.frag -o ${CMAKE_BINARY_DIR}/shaders/frag.spv
)
add_dependencies(LillUgsi Shaders)