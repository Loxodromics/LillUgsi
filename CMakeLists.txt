cmake_minimum_required(VERSION 3.24)
project(LillUgsi VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_NDEBUG "Enable NDEBUG macro" OFF)

if(ENABLE_NDEBUG)
	add_compile_definitions(NDEBUG)
endif()

if(NOT DEFINED Vulkan_DIR)
	message(STATUS "Vulkan_DIR not specified, trying to find Vulkan automatically")
else()
	message(STATUS "Using specified Vulkan_DIR: ${Vulkan_DIR}")
endif()

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
		src/scene/scenenode.cpp
		src/scene/scene.cpp
		src/rendering/light.cpp
		src/rendering/lightmanager.cpp
		src/rendering/pbrmaterial.cpp
		src/rendering/custommaterial.cpp
		src/rendering/materialmanager.cpp
		src/rendering/material.cpp
		src/rendering/icospheremesh.cpp
		src/vulkan/pipelineconfig.cpp
		src/rendering/wireframematerial.cpp
		src/planet/datasettingvisitor.cpp
		src/planet/face.cpp
		src/planet/planetdata.cpp
		src/planet/vertexdata.cpp
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
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.glsl.vert -o ${CMAKE_BINARY_DIR}/shaders/pbr.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.glsl.frag -o ${CMAKE_BINARY_DIR}/shaders/pbr.frag.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/wireframe.glsl.vert -o ${CMAKE_BINARY_DIR}/shaders/wireframe.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/wireframe.glsl.frag -o ${CMAKE_BINARY_DIR}/shaders/wireframe.frag.spv
)
add_dependencies(LillUgsi Shaders)