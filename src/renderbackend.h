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
    virtual void CreateApiInstance()  = 0;
    virtual void DestroyApiInstance() = 0;

    // TODO: extensive explanation goes here.
    virtual void CreateDisplaySurface(const Window& window) = 0;
    virtual void DestroyDisplaySurface() = 0;

    // TODO: extensive explanation goes here.
    virtual void CreateGraphicsDevice()  = 0;
    virtual void DestroyGraphicsDevice() = 0;

    // TODO: extensive explanation goes here.
    virtual void CreateSyncPrimitives()  = 0;
    virtual void DestroySyncPrimitives() = 0;

    // TODO: extensive explanation goes here.
    virtual void CreateSwapChain()  = 0;
    virtual void DestroySwapChain() = 0;
};

struct VulkanInstanceProperties
{
    uint32_t                      enabledLayerCount;
    string_t*                     enabledLayers;

    uint32_t                      supportedExtensionCount;
    VkExtensionProperties*        supportedExtensions;

    uint32_t                      activeExtensionCount;
    string_t*                     activeExtensions;
};

struct VulkanDeviceProperties
{
    VkPhysicalDevice              physicalDevice;
    VkPhysicalDeviceProperties    physicalDeviceProperties;
    VkPhysicalDeviceFeatures      physicalDeviceFeatures;

    uint32_t                      supportedExtensionCount;
    VkExtensionProperties*        supportedExtensions;

    uint32_t                      activeExtensionCount;
    string_t*                     activeExtensions;

    uint32_t                      queueFamilyCount;
    VkQueueFamilyProperties*      queueFamilies;                      
};

struct VulkanSwapChainProperties
{
    VkSurfaceCapabilitiesKHR      surfaceCapabilities;

    uint32_t                      surfaceFormatCount;
    VkSurfaceFormatKHR*           surfaceFormats;

    uint32_t                      presentModeCount;
    VkPresentModeKHR*             presentModes;

    VkImageUsageFlags             activeSurfaceUsageFlags;
    VkSurfaceTransformFlagBitsKHR activeSurfaceTransforms;
    VkSurfaceFormatKHR            activeSurfaceFormat;
    VkPresentModeKHR              activePresentMode;
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
    virtual void CreateSwapChain()       final;
    virtual void DestroySwapChain()      final;

private:

    VulkanInstanceProperties  GetInstanceProperties()  const;
    VulkanDeviceProperties    GetDeviceProperties()    const;
    VulkanSwapChainProperties GetSwapChainProperties() const;

private:

    // Frequently-accessed working parts.
    VkAllocationCallbacks*    allocator;
    VkInstance                instance;
    VkDevice                  device;
    VkQueue                   graphicsQueue;
    VkQueue                   computeQueue;
    VkQueue                   transferQueue;
    VkQueue                   presentQueue;
    VkSemaphore               semaphore;
    VkSurfaceKHR              surface;
    VkExtent2D                surfaceDimensions;
    uint32_t                  bufferCount;
    VkSwapchainKHR            swapChain;

    // Rarely-accessed introspection parts.
    VulkanInstanceProperties  instanceProperties;
    VulkanDeviceProperties    deviceProperties;
    VulkanSwapChainProperties swapChainProperties;
};
