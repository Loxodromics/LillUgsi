cmake_minimum_required(VERSION 3.24)
project(LillUgsi VERSION 0.2.1)

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
find_package(FastNoise2 REQUIRED)
find_package(stb REQUIRED)
find_package(tinygltf REQUIRED)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# Define the main executable sources without planet files
set(MAIN_SOURCES
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
		src/rendering/terrainmaterial.cpp
		src/rendering/screenshot.cpp
		src/rendering/texture.cpp
		src/rendering/textureloader.cpp
		src/rendering/texturemanager.cpp
		src/rendering/orbitcamera.cpp
		src/vulkan/commandbuffermanager.cpp
		src/vulkan/framebuffermanager.cpp
		src/rendering/buffermanager.cpp
		src/rendering/models/modelmanager.cpp
		src/rendering/models/gltfmodelloader.cpp
		src/rendering/models/meshextractor.cpp
		src/rendering/models/materialextractor.cpp
)

add_executable(LillUgsi ${MAIN_SOURCES})

# Check if the planet module is available
set(PLANET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/planet")
option(USE_PLANET "Enable planet generation features" ON)

# Add planet module if available and enabled
if(USE_PLANET AND EXISTS "${PLANET_DIR}")
	message(STATUS "Planet module found at ${PLANET_DIR}, including it in the build")
	add_subdirectory(${PLANET_DIR})
	target_link_libraries(LillUgsi PRIVATE LillUgsiPlanet)
	target_compile_definitions(LillUgsi PRIVATE USE_PLANET)
	target_include_directories(LillUgsi PRIVATE
			${PLANET_DIR}/src
	)
else()
	message(STATUS "Planet module not found or disabled")
endif()

# Rest of your target settings
target_include_directories(LillUgsi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${Vulkan_INCLUDE_DIRS})

target_link_libraries(LillUgsi
		PRIVATE
		SDL3::SDL3
		spdlog::spdlog
		Vulkan::Vulkan
		glm::glm
		FastNoise2::FastNoise
		stb::stb
		TinyGLTF::TinyGLTF
)

# Add resource copying
set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

# Check if resources directory exists
if(EXISTS ${RESOURCES_DIR})
    message(STATUS "Resources directory found at ${RESOURCES_DIR}, setting up copying")
    
    # Create custom command to copy resources
    add_custom_command(
        TARGET LillUgsi
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RESOURCES_DIR}
            $<TARGET_FILE_DIR:LillUgsi>/resources
        COMMENT "Copying resources directory to build output"
    )
else()
    message(WARNING "Resources directory not found at ${RESOURCES_DIR}")
endif()

add_custom_target(Shaders
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.glsl.vert -o ${CMAKE_BINARY_DIR}/shaders/pbr.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.glsl.frag -o ${CMAKE_BINARY_DIR}/shaders/pbr.frag.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/wireframe.glsl.vert -o ${CMAKE_BINARY_DIR}/shaders/wireframe.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/wireframe.glsl.frag -o ${CMAKE_BINARY_DIR}/shaders/wireframe.frag.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.glsl.vert -o ${CMAKE_BINARY_DIR}/shaders/terrain.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.glsl.frag -o ${CMAKE_BINARY_DIR}/shaders/terrain.frag.spv
)
add_dependencies(LillUgsi Shaders)
