#define PLATFORM_WIN32

// Clean up Windows header includes.
#define NOMINMAX
#define STRICT
#define WIN32_LEAN_AND_MEAN

// API-specific details. Define -before- including API headers.
#ifdef PLATFORM_WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif
#define VK_MAX_LAYERS              8
#define VK_MAX_DEVICES             8
#define VK_MAX_QUEUE_FAMILIES      4
#define VK_REQ_INSTANCE_EXTENSIONS 2
#define VK_REQ_DEVICE_EXTENSIONS   1

#include <cassert>
#include <cstring>
#include <memory>
#include <vulkan/vulkan.h>

#include "utility.h"
#include "window.h"

// API-specific details. Define -after- including API headers.
#ifdef PLATFORM_WIN32
    #define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif

VkResult vkCreateSurfaceKHR(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
#ifdef PLATFORM_WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};

    return vkCreateWin32SurfaceKHR(instance, &surfaceInfo, pAllocator, pSurface);  
#endif
}

bool ContainsExtension(string_t name, const VkExtensionProperties list[], size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(name, list[i].extensionName) == 0)
        {
            return true;
        }
    }

    return false;
}

int main(const int argc, string_t argv[])
{
    ASSERT(argc == 3, "Provide the rendering resolution (width followed by height) in the command line.");

    uint16_t windowWidth  = static_cast<uint16_t>(atoi(argv[1]));
    uint16_t windowHeight = static_cast<uint16_t>(atoi(argv[2]));

    VkApplicationInfo appInfo  = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName        = "Magma Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pApplicationName   = appInfo.pEngineName;
    appInfo.applicationVersion = appInfo.engineVersion;
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    uint32_t layerCount = 0;
    string_t layers[VK_MAX_LAYERS];
    
#if defined(_DEBUG)
    // Use validation layers if this is a debug build.
    layers[layerCount++] = "VK_LAYER_LUNARG_standard_validation";
#endif

    string_t requiredInstanceExtensions[VK_REQ_INSTANCE_EXTENSIONS] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_PLATFORM_SURFACE_EXTENSION_NAME };
    string_t requiredDeviceExtensions[VK_REQ_DEVICE_EXTENSIONS]     = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    uint32_t instanceExtensionCount;
    CHECK_INT(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr),
              "Failed to enumerate extensions supported by the graphics API.");

    // Note: VkExtensionProperties is large, so we have to allocate on the heap.
    auto instanceExtensions = std::make_unique<VkExtensionProperties[]>(instanceExtensionCount);
    CHECK_INT(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.get()),
              "Failed to enumerate extensions supported by the graphics API.");

    // Check whether all the required extensions are supported.
    for (string_t extName : requiredInstanceExtensions)
    {
        ASSERT(ContainsExtension(extName, instanceExtensions.get(), instanceExtensionCount),
               "The extension \'%s\' is not supported by the graphics API.", extName);
    }

    VkInstanceCreateInfo instanceInfo    = {};
    instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo        = &appInfo;
    instanceInfo.enabledLayerCount       = layerCount;
    instanceInfo.ppEnabledLayerNames     = layers;
    instanceInfo.enabledExtensionCount   = VK_REQ_INSTANCE_EXTENSIONS;
    instanceInfo.ppEnabledExtensionNames = requiredInstanceExtensions;

    VkAllocationCallbacks* allocator = nullptr; // TODO

    VkInstance instance;
    CHECK_INT(vkCreateInstance(&instanceInfo, allocator, &instance),
              "Failed to create a graphics API instance.");

    uint32_t physicalDeviceCount;
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr),
              "Failed to enumerate physical graphics devices.");

    VkPhysicalDevice physicalDevices[VK_MAX_DEVICES];
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices),
              "Failed to enumerate physical graphics devices.");

    VkPhysicalDevice         physicalDevice = nullptr;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
              
    uint32_t graphicsQueueFamilyIndex       = UINT32_MAX;
    uint32_t computeQueueFamilyIndex        = UINT32_MAX;
    uint32_t transferQueueFamilyIndex       = UINT32_MAX;
    uint32_t sparseResourceQueueFamilyIndex = UINT32_MAX;

    // Select a physical device, and query its properties.
    for (uint32_t d = 0; d < physicalDeviceCount; d++)
    {
        VkPhysicalDevice device = physicalDevices[d];

        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures   deviceFeatures;

        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(  device, &deviceFeatures);
        
        // Select the first compatible GPU.
        if (deviceProperties.apiVersion >= appInfo.apiVersion &&
            deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            physicalDevice         = device;
            physicalDeviceFeatures = deviceFeatures;

            uint32_t queueFamilyCount;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            VkQueueFamilyProperties queueFamilies[VK_MAX_QUEUE_FAMILIES];
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

            // Enumerate all queue families.
            for (uint32_t q = 0; q < queueFamilyCount; q++)
            {
                VkQueueFlags queueFlags = queueFamilies[q].queueFlags;

                constexpr VkQueueFlags primaryQueueFlags = VK_QUEUE_GRAPHICS_BIT
                                                         | VK_QUEUE_COMPUTE_BIT
                                                         | VK_QUEUE_TRANSFER_BIT
                                                         | VK_BUFFER_CREATE_SPARSE_BINDING_BIT;

                // We rely on the graphics queue to support all functionality.
                if ((queueFlags & primaryQueueFlags) == primaryQueueFlags)
                {
                    graphicsQueueFamilyIndex = q;
                }
                else if (queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    computeQueueFamilyIndex = q;
                }
                else if (queueFlags & VK_QUEUE_TRANSFER_BIT)
                {
                    transferQueueFamilyIndex = q;
                }
                else if (queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
                {
                    sparseResourceQueueFamilyIndex = q;
                }
            }

            uint32_t deviceExtensionCount;
            CHECK_INT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr),
                      "Failed to enumerate extensions supported by the graphics device.");

            // Note: VkExtensionProperties is large, so we have to allocate on the heap.
            auto deviceExtensions = std::make_unique<VkExtensionProperties[]>(deviceExtensionCount);
            CHECK_INT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, deviceExtensions.get()),
                      "Failed to enumerate extensions supported by the graphics device.");

            // Check whether all the required extensions are supported.
            for (string_t extName : requiredDeviceExtensions)
            {
                ASSERT(ContainsExtension(extName, deviceExtensions.get(), deviceExtensionCount),
                       "The extension \'%s\' is not supported by the graphics device.", extName);
            }
        }
    }

    ASSERT(physicalDevice, "Failed to find a compatible physical graphics device.");

    // Create one queue per family.
    uint32_t                queueCount = 0;
    VkDeviceQueueCreateInfo queueInfos[VK_MAX_QUEUE_FAMILIES] = {};

    constexpr float normalPriority = 1.0f;

    if (graphicsQueueFamilyIndex != UINT32_MAX)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = graphicsQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &normalPriority;        

        queueCount++;
    }
    else
    {
        CHECK_INT(graphicsQueueFamilyIndex, "Failed to find the graphics queue index.");
    }

    if (computeQueueFamilyIndex != UINT32_MAX)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = computeQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &normalPriority;        

        queueCount++;
    }

    if (transferQueueFamilyIndex != UINT32_MAX)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = transferQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &normalPriority;        

        queueCount++;
    }

    if (sparseResourceQueueFamilyIndex != UINT32_MAX)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = sparseResourceQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &normalPriority;        

        queueCount++;
    }

    // Create a logical device. Enable all supported features.
    VkDeviceCreateInfo deviceInfo      = {};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount    = queueCount;
    deviceInfo.pQueueCreateInfos       = queueInfos;
    deviceInfo.enabledLayerCount       = instanceInfo.enabledLayerCount;
    deviceInfo.ppEnabledLayerNames     = instanceInfo.ppEnabledLayerNames;
    deviceInfo.enabledExtensionCount   = VK_REQ_DEVICE_EXTENSIONS;
    deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
    deviceInfo.pEnabledFeatures        = &physicalDeviceFeatures;

    VkDevice virtualDevice;
    CHECK_INT(vkCreateDevice(physicalDevice, &deviceInfo, allocator, &virtualDevice),
              "Failed to create a virtual graphics device.");

    VkQueue graphicsQueue = nullptr;
    VkQueue computeQueue;
    VkQueue transferQueue;
    VkQueue sparseResourceQueue;

    if (graphicsQueueFamilyIndex != UINT32_MAX)
    {
        vkGetDeviceQueue(virtualDevice, graphicsQueueFamilyIndex, 0, &graphicsQueue);
    }

    if (computeQueueFamilyIndex != UINT32_MAX)
    {
        vkGetDeviceQueue(virtualDevice, computeQueueFamilyIndex, 0, &computeQueue);
    }
    else
    {
        // No async compute, fall back to the graphics queue.
        computeQueue = graphicsQueue;
    }

    if (transferQueueFamilyIndex != UINT32_MAX)
    {
        vkGetDeviceQueue(virtualDevice, transferQueueFamilyIndex, 0, &transferQueue);
    }
    else
    {
        // No async transfers, fall back to the graphics queue.
        transferQueue = graphicsQueue;
    }

    if (sparseResourceQueueFamilyIndex != UINT32_MAX)
    {
        vkGetDeviceQueue(virtualDevice, sparseResourceQueueFamilyIndex, 0, &sparseResourceQueue);
    }
    else
    {
        // No async sparse resource management, fall back to the graphics queue.
        sparseResourceQueue = graphicsQueue;
    }

    Window::open(windowHeight, windowWidth);

    // Clean up.
    // API note: you only have to vkDestroy() objects you vkCreate().
    vkDeviceWaitIdle(virtualDevice);
    vkDestroyDevice(virtualDevice, allocator);
    vkDestroyInstance(instance, allocator);

    return EXIT_SUCCESS;
}
