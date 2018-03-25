#include <cassert>
#include <vulkan/vulkan.h>

#define VK_MAX_LAYERS         8
#define VK_MAX_DEVICES        8
#define VK_MAX_QUEUE_FAMILIES 4

#include "utility.h"

int main(const int argc, string_t argv[])
{
    argc, argv; // TODO

    // Use validation layers if this is a debug build.
    string_t layers[VK_MAX_LAYERS];
    uint32_t layerCount = 0;
    
#if defined(_DEBUG)
    layers[layerCount++] = "VK_LAYER_LUNARG_standard_validation";
#endif

    VkApplicationInfo appInfo  = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName        = "Magma Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pApplicationName   = appInfo.pEngineName;
    appInfo.applicationVersion = appInfo.engineVersion;
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceInfo    = {};
    instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo        = &appInfo;
    instanceInfo.enabledLayerCount       = layerCount;
    instanceInfo.ppEnabledLayerNames     = layers;
    instanceInfo.enabledExtensionCount   = 0;       // TODO
    instanceInfo.ppEnabledExtensionNames = nullptr; // TODO

    VkAllocationCallbacks* allocator = nullptr; // TODO

    VkInstance instance;
    CHECK_INT(vkCreateInstance(&instanceInfo, allocator, &instance),
              "Failed to create a Vulkan instance.");

    uint32_t         physicalDeviceCount;
    VkPhysicalDevice physicalDevices[VK_MAX_DEVICES];
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr),
              "Failed to enumerate physical graphics devices.");
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices),
              "Failed to enumerate physical graphics devices.");

    VkPhysicalDevice         physicalDevice = nullptr;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
              
    uint32_t queueFamilyCount;
    uint32_t graphicsQueueFamilyIndex       = UINT32_MAX;
    uint32_t computeQueueFamilyIndex        = UINT32_MAX;
    uint32_t transferQueueFamilyIndex       = UINT32_MAX;
    uint32_t sparseResourceQueueFamilyIndex = UINT32_MAX;

    // Select a physical device, and query its properties.
    for (uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDevice device = physicalDevices[i];

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

            VkQueueFamilyProperties queueFamilies[VK_MAX_QUEUE_FAMILIES];
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

            // Enumerate all queue family types.
            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                VkQueueFlags queueFlags = queueFamilies[j].queueFlags;

                constexpr VkQueueFlags primaryQueueFlags = VK_QUEUE_GRAPHICS_BIT |
                                                           VK_QUEUE_COMPUTE_BIT  |
                                                           VK_QUEUE_TRANSFER_BIT |
                                                           VK_BUFFER_CREATE_SPARSE_BINDING_BIT;

                // We rely on the graphics queue to support all functionality.
                if ((queueFlags & primaryQueueFlags) == primaryQueueFlags)
                {
                    graphicsQueueFamilyIndex = j;
                }
                else if (queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    computeQueueFamilyIndex = j;
                }
                else if (queueFlags & VK_QUEUE_TRANSFER_BIT)
                {
                    transferQueueFamilyIndex = j;
                }
                else if (queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
                {
                    sparseResourceQueueFamilyIndex = j;
                }
            }
        }
    }

    CHECK_PTR(physicalDevice, "Failed to find a compatible physical graphics device.");

    // Create one queue per family.
    VkDeviceQueueCreateInfo queueInfos[VK_MAX_QUEUE_FAMILIES] = {};
    uint32_t queueCount = 0;

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

    assert(queueCount == queueFamilyCount);

    // Create a logical device. Enable all supported features.
    VkDeviceCreateInfo deviceInfo      = {};
    deviceInfo.queueCreateInfoCount    = queueCount;
    deviceInfo.pQueueCreateInfos       = queueInfos;
    deviceInfo.enabledLayerCount       = instanceInfo.enabledLayerCount;
    deviceInfo.ppEnabledLayerNames     = instanceInfo.ppEnabledLayerNames;
    deviceInfo.enabledExtensionCount   = instanceInfo.enabledExtensionCount;
    deviceInfo.ppEnabledExtensionNames = instanceInfo.ppEnabledExtensionNames;
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

    // Clean up.
    // API note: you only have to vkDestroy() objects you vkCreate().
    vkDeviceWaitIdle(virtualDevice);
    vkDestroyDevice(virtualDevice, allocator);
    vkDestroyInstance(instance, allocator);

    return EXIT_SUCCESS;
}
