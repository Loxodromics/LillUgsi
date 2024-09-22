#pragma once

#include <vulkan/vulkan.h>
#include <functional>

/// Template class for RAII management of Vulkan objects
template<typename T, typename Deleter>
class VulkanHandle {
public:
	VulkanHandle() : handle(VK_NULL_HANDLE), deleter(nullptr) {}

	VulkanHandle(T handle, Deleter deleter) : handle(handle), deleter(deleter) {}

	/// Move constructor
	VulkanHandle(VulkanHandle&& other) noexcept : handle(other.handle), deleter(std::move(other.deleter)) {
		other.handle = VK_NULL_HANDLE;
	}

	/// Move assignment operator
	VulkanHandle& operator=(VulkanHandle&& other) noexcept {
		if (this != &other) {
			this->reset();
			this->handle = other.handle;
			this->deleter = std::move(other.deleter);
			other.handle = VK_NULL_HANDLE;
		}
		return *this;
	}

	/// Destructor
	~VulkanHandle() {
		this->reset();
	}

	/// Reset the handle, cleaning up the resource if necessary
	void reset(T newHandle = VK_NULL_HANDLE, Deleter newDeleter = nullptr) {
		if (this->handle != VK_NULL_HANDLE && this->deleter) {
			this->deleter(this->handle);
		}
		this->handle = newHandle;
		this->deleter = newDeleter;
	}

	/// Get the raw handle
	T get() const { return this->handle; }

	/// Check if the handle is valid
	bool isValid() const { return this->handle != VK_NULL_HANDLE; }

	/// Implicit conversion to T
	operator T() const { return this->handle; }

	/// Deleted copy constructor and assignment operator to enforce move semantics
	VulkanHandle(const VulkanHandle&) = delete;
	VulkanHandle& operator=(const VulkanHandle&) = delete;

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