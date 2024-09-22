#include "vulkan/vulkaninstance.h"
#include <spdlog/spdlog.h>
#include <vector>
#include <iostream>

int main() {
    try {
        // Initialize spdlog
        spdlog::set_level(spdlog::level::debug);
        spdlog::info("Starting Vulkan Learning Renderer");

        // Create VulkanInstance
        VulkanInstance vulkanInstance;

        // Define required extensions
        // In a real application, you'd get these from SDL or your windowing system
        std::vector<const char*> requiredExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            // Add platform-specific surface extension here, e.g.:
            // VK_KHR_WIN32_SURFACE_EXTENSION_NAME for Windows
            // VK_KHR_XCB_SURFACE_EXTENSION_NAME for Linux with XCB
            // VK_MVK_MACOS_SURFACE_EXTENSION_NAME for macOS
        };

        // Initialize Vulkan instance
        if (!vulkanInstance.initialize(requiredExtensions)) {
            spdlog::error("Failed to initialize Vulkan instance");
            return 1;
        }

        spdlog::info("Vulkan instance created successfully");

        // Get the instance handle (just to demonstrate it works)
        VkInstance instance = vulkanInstance.getInstance();
        if (instance == VK_NULL_HANDLE) {
            spdlog::error("Retrieved Vulkan instance handle is null");
            return 1;
        }

        spdlog::info("Vulkan instance handle retrieved successfully");

        // Your rendering code would go here...

        spdlog::info("Exiting application");
    } catch (const std::exception& e) {
        spdlog::error("Caught exception: {}", e.what());
        return 1;
    }

    return 0;
}