// Clean up Windows header includes.
#define NOMINMAX
#define STRICT
#define WIN32_LEAN_AND_MEAN

// API-specific details. Define -before- including API headers.
#ifdef WIN32
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
#ifdef WIN32
    #define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif

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

VkResult vkCreateSurfaceKHR(VkInstance instance, const void* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
#ifdef WIN32
    return vkCreateWin32SurfaceKHR(instance, static_cast<const VkWin32SurfaceCreateInfoKHR*>(pCreateInfo), pAllocator, pSurface);  
#endif
}

int main(const int argc, string_t argv[])
{
    ASSERT(argc == 3, "Missing command line agruments: resolution. E.g.: 1920 1080.");

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
    
#ifdef _DEBUG
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

    // Create the OS window used for drawing. Needed to create the swap chain. 
    Window::Create(windowHeight, windowWidth);

#ifdef WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = Window::Instance();
    surfaceInfo.hwnd      = Window::Handle();
#endif

    VkSurfaceKHR surface;
    CHECK_INT(vkCreateSurfaceKHR(instance, &surfaceInfo, allocator, &surface),
              "Failed to create a presentation surface.");

    uint32_t physicalDeviceCount;
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr),
              "Failed to enumerate physical graphics devices.");

    VkPhysicalDevice physicalDevices[VK_MAX_DEVICES];
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices),
              "Failed to enumerate physical graphics devices.");

    VkPhysicalDevice         physicalDevice = nullptr;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
              
    uint32_t graphicsQueueFamilyIndex, computeQueueFamilyIndex, transferQueueFamilyIndex, presentQueueFamilyIndex;

    // Select a physical device, and query its properties.
    for (uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDevice device = physicalDevices[i];
        
        uint32_t deviceExtensionCount;
        CHECK_INT(vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount, nullptr),
                  "Failed to enumerate extensions supported by the graphics device.");

        // Note: VkExtensionProperties is large, so we have to allocate on the heap.
        auto deviceExtensions = std::make_unique<VkExtensionProperties[]>(deviceExtensionCount);
        CHECK_INT(vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount, deviceExtensions.get()),
                  "Failed to enumerate extensions supported by the graphics device.");

        bool deviceSupportsRequiredExtensions = true;

        // Check whether all the required extensions are supported.
        for (string_t extName : requiredDeviceExtensions)
        {
            deviceSupportsRequiredExtensions &= ContainsExtension(extName, deviceExtensions.get(), deviceExtensionCount);
        }

        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        VkQueueFamilyProperties queueFamilies[VK_MAX_QUEUE_FAMILIES];
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

        // Enumerate all queue families.
        graphicsQueueFamilyIndex = computeQueueFamilyIndex = transferQueueFamilyIndex = presentQueueFamilyIndex = UINT32_MAX;

        for (uint32_t q = 0; q < queueFamilyCount; q++)
        {
            VkBool32 queueSupportsPresentation;
            CHECK_INT(vkGetPhysicalDeviceSurfaceSupportKHR(device, q, surface, &queueSupportsPresentation),
                      "Failed to query the graphics device surface support.");

            VkQueueFlags queueFlags = queueFamilies[q].queueFlags;

            constexpr VkQueueFlags primaryQueueFlags = VK_QUEUE_GRAPHICS_BIT
                                                     | VK_QUEUE_COMPUTE_BIT
                                                     | VK_QUEUE_TRANSFER_BIT;

            // We rely on the graphics queue to support all functionality.
            if ((queueFlags & primaryQueueFlags) == primaryQueueFlags)
            {
                graphicsQueueFamilyIndex = q;

                // Use the graphics queue to present, if possible.
                if (queueSupportsPresentation)
                {
                    presentQueueFamilyIndex = q;
                }
            }
            else if (queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                computeQueueFamilyIndex = q;
            }
            else if (queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                transferQueueFamilyIndex = q;
            }
            else if (queueSupportsPresentation && presentQueueFamilyIndex != UINT32_MAX)
            {
                // If the graphics queue does not support presentation,
                // use the dedicated presentation queue.
                presentQueueFamilyIndex = q;
            }
        }

        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures   deviceFeatures;

        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(  device, &deviceFeatures);

        // Select the first compatible GPU.
        if (deviceProperties.apiVersion >= appInfo.apiVersion &&
            deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            deviceSupportsRequiredExtensions &&
            graphicsQueueFamilyIndex != UINT32_MAX &&
            presentQueueFamilyIndex != UINT32_MAX)
        {
            physicalDevice         = device;
            physicalDeviceFeatures = deviceFeatures;

            break;
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

    if (presentQueueFamilyIndex != graphicsQueueFamilyIndex)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = presentQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &normalPriority;        

        queueCount++;
    }

    // Create a virtual device. Enable all supported features.
    VkDeviceCreateInfo virtualDeviceInfo      = {};
    virtualDeviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    virtualDeviceInfo.queueCreateInfoCount    = queueCount;
    virtualDeviceInfo.pQueueCreateInfos       = queueInfos;
    virtualDeviceInfo.enabledLayerCount       = instanceInfo.enabledLayerCount;
    virtualDeviceInfo.ppEnabledLayerNames     = instanceInfo.ppEnabledLayerNames;
    virtualDeviceInfo.enabledExtensionCount   = VK_REQ_DEVICE_EXTENSIONS;
    virtualDeviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
    virtualDeviceInfo.pEnabledFeatures        = &physicalDeviceFeatures;

    VkDevice virtualDevice;
    CHECK_INT(vkCreateDevice(physicalDevice, &virtualDeviceInfo, allocator, &virtualDevice),
              "Failed to create a virtual graphics device.");

    VkQueue graphicsQueue, computeQueue, transferQueue, presentQueue;

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

    if (presentQueueFamilyIndex != graphicsQueueFamilyIndex)
    {
        // Use a dedicated presentation queue.
        vkGetDeviceQueue(virtualDevice, presentQueueFamilyIndex, 0, &graphicsQueue);
    }
    else
    {
        presentQueue = graphicsQueue;
    }

    // Clean up.
    // API note: you only have to vkDestroy() objects you vkCreate().
    vkDeviceWaitIdle(virtualDevice);
    vkDestroyDevice(virtualDevice, allocator);
    vkDestroyInstance(instance, allocator);

    return EXIT_SUCCESS;
}
