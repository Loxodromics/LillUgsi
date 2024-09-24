#pragma once

#include "vulkanhandle.h"
#include <vulkan/vulkan.h>
#include <functional>

/// Type alias for VkInstance wrapper
using VulkanInstanceHandle = VulkanHandle<VkInstance, std::function<void(VkInstance)>>;

/// Type alias for VkDevice wrapper
using VulkanDeviceHandle = VulkanHandle<VkDevice, std::function<void(VkDevice)>>;

/// Type alias for VkSwapchainKHR wrapper
using VulkanSwapchainHandle = VulkanHandle<VkSwapchainKHR, std::function<void(VkSwapchainKHR)>>;

/// Type alias for VkSurfaceKHR wrapper
using VulkanSurfaceHandle = VulkanHandle<VkSurfaceKHR, PFN_vkDestroySurfaceKHR>;

/// Type alias for VkShaderModule wrapper
using VulkanShaderModuleHandle = VulkanHandle<VkShaderModule, PFN_vkDestroyShaderModule>;

/// Type alias for VkPipeline wrapper
using VulkanPipelineHandle = VulkanHandle<VkPipeline, PFN_vkDestroyPipeline>;

/// Type alias for VkPipelineLayout wrapper
using VulkanPipelineLayoutHandle = VulkanHandle<VkPipelineLayout, PFN_vkDestroyPipelineLayout>;

/// Type alias for VkRenderPass wrapper
using VulkanRenderPassHandle = VulkanHandle<VkRenderPass, PFN_vkDestroyRenderPass>;

/// Type alias for VkFramebuffer wrapper
using VulkanFramebufferHandle = VulkanHandle<VkFramebuffer, PFN_vkDestroyFramebuffer>;

/// Type alias for VkCommandPool wrapper
using VulkanCommandPoolHandle = VulkanHandle<VkCommandPool, PFN_vkDestroyCommandPool>;

/// Type alias for VkSemaphore wrapper
using VulkanSemaphoreHandle = VulkanHandle<VkSemaphore, PFN_vkDestroySemaphore>;

/// Type alias for VkFence wrapper
using VulkanFenceHandle = VulkanHandle<VkFence, PFN_vkDestroyFence>;

/// Type alias for VkBuffer wrapper
using VulkanBufferHandle = VulkanHandle<VkBuffer, PFN_vkDestroyBuffer>;

/// Type alias for VkImage wrapper
using VulkanImageHandle = VulkanHandle<VkImage, PFN_vkDestroyImage>;

/// Type alias for VkImageView wrapper
using VulkanImageViewHandle = VulkanHandle<VkImageView, std::function<void(VkImageView)>>;

/// Type alias for VkSampler wrapper
using VulkanSamplerHandle = VulkanHandle<VkSampler, PFN_vkDestroySampler>;

/// Type alias for VkDescriptorSetLayout wrapper
using VulkanDescriptorSetLayoutHandle = VulkanHandle<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>;

/// Type alias for VkDescriptorPool wrapper
using VulkanDescriptorPoolHandle = VulkanHandle<VkDescriptorPool, PFN_vkDestroyDescriptorPool>;

/// Function to create a VkInstance with proper error handling
VkResult createVulkanInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VulkanInstanceHandle& instance);

/// Function to create a VkDevice with proper error handling
VkResult createVulkanDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VulkanDeviceHandle& device);

/// Function to create a VkSwapchainKHR with proper error handling
VkResult createVulkanSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VulkanSwapchainHandle& swapchain);