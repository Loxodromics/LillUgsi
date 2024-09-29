#pragma once

#include <vulkan/vulkan.h>
#include <functional>

/// Template class for RAII management of Vulkan objects
template<typename T, typename Deleter>
class VulkanHandle {
public:
	VulkanHandle() : handle(VK_NULL_HANDLE), deleter(nullptr) {}
	VulkanHandle(T handle, Deleter deleter) : handle(handle), deleter(deleter) {}

	// Disable copying
	VulkanHandle(const VulkanHandle&) = delete;
	VulkanHandle& operator=(const VulkanHandle&) = delete;

	// Enable moving
	VulkanHandle(VulkanHandle&& other) noexcept : handle(other.handle), deleter(std::move(other.deleter)) {
		other.handle = VK_NULL_HANDLE;
	}
	VulkanHandle& operator=(VulkanHandle&& other) noexcept {
		if (this != &other) {
			reset();
			handle = other.handle;
			deleter = std::move(other.deleter);
			other.handle = VK_NULL_HANDLE;
		}
		return *this;
	}

	~VulkanHandle() { reset(); }

	void reset(T newHandle = VK_NULL_HANDLE, Deleter newDeleter = nullptr) {
		if (handle != VK_NULL_HANDLE && deleter) {
			deleter(handle);
		}
		handle = newHandle;
		deleter = newDeleter;
	}

	T get() const { return handle; }
	bool isValid() const { return handle != VK_NULL_HANDLE; }
	operator T() const { return handle; }

private:
	T handle;
	Deleter deleter;
};

/// Specialized wrapper for VkInstance
class VulkanInstanceWrapper : public VulkanHandle<VkInstance, std::function<void(VkInstance)>> {
public:
	VulkanInstanceWrapper() : VulkanHandle() {}

	VulkanInstanceWrapper(VkInstance instance, PFN_vkDestroyInstance deleter)
		: VulkanHandle(instance, [deleter](VkInstance i) { deleter(i, nullptr); }) {}

	/// Create a new Vulkan instance
	VkResult create(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator = nullptr) {
		VkInstance instance;
		VkResult result = vkCreateInstance(pCreateInfo, pAllocator, &instance);
		if (result == VK_SUCCESS) {
			this->reset(instance, [pAllocator](VkInstance i) { vkDestroyInstance(i, pAllocator); });
		}
		return result;
	}
};