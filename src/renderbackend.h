#pragma once

#include "definitions.h"

#ifdef WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>

class Window;

// Interface - abstract (base) class containing only pure virtual functions.
class RenderBackEnd
{
public:

    // TODO: extensive explanation goes here.
    virtual void CreateApiInstance()     = 0;
    virtual void DestroyApiInstance()    = 0;

    // TODO: extensive explanation goes here.
    virtual void CreateDisplaySurface(const Window& window)  = 0;
    virtual void DestroyDisplaySurface() = 0;

    // TODO: extensive explanation goes here.
    virtual void CreateGraphicsDevice()  = 0;
    virtual void DestroyGraphicsDevice() = 0;

    // TODO: extensive explanation goes here.
    virtual void CreateSyncPrimitives()  = 0;
    virtual void DestroySyncPrimitives() = 0;
};

struct VulkanInstanceProperties
{
    uint32_t                   enabledLayerCount;
    string_t*                  enabledLayers;

    uint32_t                   supportedExtensionCount;
    VkExtensionProperties*     supportedExtensions;

    uint32_t                   enabledExtensionCount;
    string_t*                  enabledExtensions;
};

struct VulkanDeviceProperties
{
    VkPhysicalDevice           physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceFeatures   physicalDeviceFeatures;

    uint32_t                   supportedExtensionCount;
    VkExtensionProperties*     supportedExtensions;

    uint32_t                   enabledExtensionCount;
    string_t*                  enabledExtensions;

    uint32_t                   queueFamilyCount;
    VkQueueFamilyProperties*   queueFamilies;                      
};

struct VulkanSwapChainProperties
{
    uint32_t                   surfaceFormatCount;
    VkSurfaceFormatKHR*        surfaceFormats;
    VkSurfaceCapabilitiesKHR   surfaceCapabilities;
};

class VulkanRenderBackEnd : public RenderBackEnd
{
public:

    VulkanRenderBackEnd();

    virtual void CreateApiInstance()     final;
    virtual void DestroyApiInstance()    final;
    virtual void CreateDisplaySurface(const Window& window) final;
    virtual void DestroyDisplaySurface() final;
    virtual void CreateGraphicsDevice()  final;
    virtual void DestroyGraphicsDevice() final;
    virtual void CreateSyncPrimitives()  final;
    virtual void DestroySyncPrimitives() final;

private:

    VulkanInstanceProperties FillInstanceProperties() const;
    VulkanDeviceProperties   FillDeviceProperties()   const;

private:

    // Frequently-accessed working parts.
    VkAllocationCallbacks*     allocator;
    VkInstance                 instance;
    VkDevice                   device;
    VkQueue                    graphicsQueue;
    VkQueue                    computeQueue;
    VkQueue                    transferQueue;
    VkQueue                    presentQueue;
    VkSemaphore                semaphore;
    VkSurfaceKHR               surface;
    VkSwapchainKHR             swapChain;

    // Rarely-accessed introspection parts.
    VulkanInstanceProperties  instanceProperties;
    VulkanDeviceProperties    deviceProperties;
    VulkanSwapChainProperties swapChainProperties;
};
