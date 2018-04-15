#include "renderbackend.h"
#include "utility.h"
#include "window.h"

#include <cassert>
#include <cstring>
#include <memory>

#define VK_API_VERSION             VK_API_VERSION_1_1

#define VK_MAX_DEVICES             8
#define VK_MAX_LAYERS              8
#define VK_MAX_QUEUE_FAMILIES      4

#define VK_REQ_INSTANCE_EXTENSIONS 2
#define VK_OPT_INSTANCE_EXTENSIONS 1

#define VK_REQ_DEVICE_EXTENSIONS   1
#define VK_OPT_DEVICE_EXTENSIONS   1

#define VK_QUEUE_PRESENT_BIT       0x01000000

#ifdef WIN32
    #define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif

bool ContainsVulkanExtension(string_t name, const VkExtensionProperties list[], size_t count)
{
    bool result = false;

    if (name)
    {
        for (size_t i = 0; i < count; i++)
        {
            if (strcmp(name, list[i].extensionName) == 0)
            {
                result = true;
                break;
            }
        }
    }

    return result;
}

VkResult vkCreateSurfaceKHR(VkInstance instance, const void* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
#ifdef WIN32
    return vkCreateWin32SurfaceKHR(instance, static_cast<const VkWin32SurfaceCreateInfoKHR*>(pCreateInfo), pAllocator, pSurface);  
#endif
}

VulkanRenderBackEnd::VulkanRenderBackEnd()
{
    allocator = nullptr; // TODO
}

VulkanInstanceProperties VulkanRenderBackEnd::FillInstanceProperties() const
{
    // Warning: these must be static so that we can take (and store) pointers to these strings.
    static string_t requiredExtensions[VK_REQ_INSTANCE_EXTENSIONS] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_PLATFORM_SURFACE_EXTENSION_NAME };
    static string_t optionalExtensions[VK_OPT_INSTANCE_EXTENSIONS] = { nullptr };

    VulkanInstanceProperties ip = {};

    // Allocate the max size up front.
    ip.enabledLayerCount = 0;
    ip.enabledLayers     = new string_t[VK_MAX_LAYERS];

#ifdef _DEBUG
    // Use validation layers if this is a debug build.
    ip.enabledLayers[ip.enabledLayerCount++] = "VK_LAYER_LUNARG_standard_validation";
#endif

    CHECK_INT(vkEnumerateInstanceExtensionProperties(nullptr, &ip.supportedExtensionCount, nullptr),
              "Failed to enumerate extensions supported by the graphics API.");

    ip.supportedExtensions = new VkExtensionProperties[ip.supportedExtensionCount];
    CHECK_INT(vkEnumerateInstanceExtensionProperties(nullptr, &ip.supportedExtensionCount, ip.supportedExtensions),
              "Failed to enumerate extensions supported by the graphics API.");

    // Allocate the max size up front.
    ip.enabledExtensionCount = 0;
    ip.enabledExtensions     = new string_t[VK_REQ_INSTANCE_EXTENSIONS + VK_OPT_INSTANCE_EXTENSIONS];

    // Check whether all the required extensions are supported.
    for (string_t extName : requiredExtensions)
    {
        ASSERT(ContainsVulkanExtension(extName, ip.supportedExtensions, ip.supportedExtensionCount),
               "The required extension \'%s\' is not supported by the graphics API.", extName);

        ip.enabledExtensions[ip.enabledExtensionCount++] = extName;
    }

    // Check whether any of the optional extensions are supported.
    for (string_t extName : optionalExtensions)
    {
        if (ContainsVulkanExtension(extName, ip.supportedExtensions, ip.supportedExtensionCount))
        {
            ip.enabledExtensions[ip.enabledExtensionCount++] = extName;
        }
    }

    return ip;
}

void VulkanRenderBackEnd::CreateApiInstance()
{
    this->instanceProperties = FillInstanceProperties();

    VkApplicationInfo appInfo  = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName        = "Magma Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pApplicationName   = appInfo.pEngineName;
    appInfo.applicationVersion = appInfo.engineVersion;
    appInfo.apiVersion         = VK_API_VERSION;

    VkInstanceCreateInfo instanceInfo    = {};
    instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo        = &appInfo;
    instanceInfo.enabledLayerCount       = instanceProperties.enabledLayerCount;
    instanceInfo.ppEnabledLayerNames     = instanceProperties.enabledLayers;
    instanceInfo.enabledExtensionCount   = instanceProperties.enabledExtensionCount;
    instanceInfo.ppEnabledExtensionNames = instanceProperties.enabledExtensions;

    CHECK_INT(vkCreateInstance(&instanceInfo, allocator, &instance),
              "Failed to create a graphics API instance.");
}

void VulkanRenderBackEnd::DestroyApiInstance()
{
    // TODO: clean up VulkanInstanceProperties.
    vkDestroyInstance(instance, allocator);
}

void VulkanRenderBackEnd::CreateDisplaySurface(const Window& window)
{
#ifdef WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = window.Instance();
    surfaceInfo.hwnd      = window.Handle();
#endif

    CHECK_INT(vkCreateSurfaceKHR(instance, &surfaceInfo, allocator, &surface),
              "Failed to create a display surface.");
}

void VulkanRenderBackEnd::DestroyDisplaySurface()
{
    vkDestroySurfaceKHR(instance, surface, allocator);
}

VulkanDeviceProperties VulkanRenderBackEnd::FillDeviceProperties() const
{
    // Warning: these must be static so that we can take (and store) pointers to these strings.
    static string_t requiredExtensions[VK_REQ_DEVICE_EXTENSIONS] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    static string_t optionalExtensions[VK_OPT_DEVICE_EXTENSIONS] = { nullptr };

    VulkanDeviceProperties dp = {};

    // Only store the selected device. None of the others are needed outside of this function.
    uint32_t physicalDeviceCount;
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr),
              "Failed to enumerate physical graphics devices.");

    VkPhysicalDevice physicalDevices[VK_MAX_DEVICES];
    CHECK_INT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices),
              "Failed to enumerate physical graphics devices.");

    // Select a physical device and query its properties.
    for (uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDevice physicalDevice = physicalDevices[i];
        
        uint32_t supportedExtensionCount;
        CHECK_INT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, nullptr),
                  "Failed to enumerate extensions supported by the graphics device.");

        auto supportedExtensions = std::make_unique<VkExtensionProperties[]>(supportedExtensionCount);
        CHECK_INT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, supportedExtensions.get()),
                  "Failed to enumerate extensions supported by the graphics device.");

        bool supportsRequiredExtensions = true;

        // Check whether all the required extensions are supported.
        for (string_t extName : requiredExtensions)
        {
            supportsRequiredExtensions &= ContainsVulkanExtension(extName, supportedExtensions.get(), supportedExtensionCount);
        }

        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        auto queueFamilies = std::make_unique<VkQueueFamilyProperties[]>(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.get());

        bool supportsGraphics     = false;
        bool supportsCompute      = false;
        bool supportsPresentation = false;

        // Determine whether the available queues cover our needs.
        for (uint32_t q = 0; q < queueFamilyCount; q++)
        {
            VkBool32 queueCanPresent;
            CHECK_INT(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, q, surface, &queueCanPresent),
                      "Failed to query the graphics device surface support.");

            if (queueCanPresent)
            {
                queueFamilies[q].queueFlags |= VK_QUEUE_PRESENT_BIT;
            }

            VkQueueFlags queueFlags = queueFamilies[q].queueFlags;

            supportsPresentation |= static_cast<bool>(queueCanPresent);
            supportsGraphics     |= static_cast<bool>(queueFlags & VK_QUEUE_GRAPHICS_BIT);
            supportsCompute      |= static_cast<bool>(queueFlags & VK_QUEUE_COMPUTE_BIT);
        }

        VkPhysicalDeviceProperties physicalDeviceProperties;
        VkPhysicalDeviceFeatures   physicalDeviceFeatures;

        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        vkGetPhysicalDeviceFeatures(  physicalDevice, &physicalDeviceFeatures);

        // Determine whether the GPU is compatible.
        if (physicalDeviceProperties.apiVersion >= VK_API_VERSION &&
            supportsRequiredExtensions &&
            supportsGraphics &&    
            supportsCompute &&     
            supportsPresentation) 
        {
            dp.physicalDevice           = physicalDevice;
            dp.physicalDeviceProperties = physicalDeviceProperties;
            dp.physicalDeviceFeatures   = physicalDeviceFeatures;

            dp.supportedExtensionCount  = supportedExtensionCount;
            dp.supportedExtensions      = supportedExtensions.release();

            dp.enabledExtensionCount    = 0;
            dp.enabledExtensions        = new string_t[VK_REQ_DEVICE_EXTENSIONS + VK_OPT_DEVICE_EXTENSIONS];

            // Copy all of the required extensions. At this point, we know that all of them are supported.
            for (string_t extName : requiredExtensions)
            {
                dp.enabledExtensions[dp.enabledExtensionCount++] = extName;
            }

            // Check whether any of the optional extensions are supported.
            for (string_t extName : optionalExtensions)
            {
                if (ContainsVulkanExtension(extName, dp.supportedExtensions, dp.supportedExtensionCount))
                {
                    dp.enabledExtensions[dp.enabledExtensionCount++] = extName;
                }
            }

            dp.queueFamilyCount = queueFamilyCount;
            dp.queueFamilies    = queueFamilies.release();

            // We found a compatible GPU, but is it the best one?
            if (physicalDeviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                // Terminate the loop over devices.
                break;
            }
        }
    }

    ASSERT(dp.physicalDevice, "Failed to find a compatible physical graphics device.");

    return dp;
}

void VulkanRenderBackEnd::CreateGraphicsDevice()
{
    this->deviceProperties = FillDeviceProperties();

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX,
             computeQueueFamilyIndex  = UINT32_MAX,
             transferQueueFamilyIndex = UINT32_MAX,
             presentQueueFamilyIndex  = UINT32_MAX;

    for (uint32_t q = 0; q < deviceProperties.queueFamilyCount; q++)
    {
        VkQueueFlags queueFlags = deviceProperties.queueFamilies[q].queueFlags;

        if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsQueueFamilyIndex = q;

            // Use the compute queue to present, if possible.
            if ((queueFlags & VK_QUEUE_PRESENT_BIT) && (presentQueueFamilyIndex == UINT32_MAX ||
                                                        presentQueueFamilyIndex != computeQueueFamilyIndex))
            {
                presentQueueFamilyIndex = q;
            }
        }
        else if (queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            computeQueueFamilyIndex = q;

            // Use the compute queue to present, if possible.
            if (queueFlags & VK_QUEUE_PRESENT_BIT)
            {
                presentQueueFamilyIndex = q;
            }
        }
        else if (queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            transferQueueFamilyIndex = q;
        }

        if ((queueFlags & VK_QUEUE_PRESENT_BIT) && (presentQueueFamilyIndex == UINT32_MAX))
        {
            // Use a dedicated presentation queue as the last resort.
            presentQueueFamilyIndex = q;
        }
    }
        
    // Create one queue per family.
    uint32_t                queueCount = 0;
    VkDeviceQueueCreateInfo queueInfos[VK_MAX_QUEUE_FAMILIES] = {};

    constexpr float highPriority = 1.0f;

    if (graphicsQueueFamilyIndex != UINT32_MAX)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = graphicsQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &highPriority;        

        queueCount++;
    }

    if (computeQueueFamilyIndex != UINT32_MAX)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = computeQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &highPriority;        

        queueCount++;
    }

    if (transferQueueFamilyIndex != UINT32_MAX)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = transferQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &highPriority;        

        queueCount++;
    }

    if (presentQueueFamilyIndex != graphicsQueueFamilyIndex &&
        presentQueueFamilyIndex != computeQueueFamilyIndex  &&
        presentQueueFamilyIndex != transferQueueFamilyIndex)
    {
        queueInfos[queueCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueCount].queueFamilyIndex = presentQueueFamilyIndex;
        queueInfos[queueCount].queueCount       = 1;
        queueInfos[queueCount].pQueuePriorities = &highPriority;        

        queueCount++;
    }

    // Create a virtual device. Enable all supported features.
    VkDeviceCreateInfo deviceInfo      = {};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount    = queueCount;
    deviceInfo.pQueueCreateInfos       = queueInfos;
    deviceInfo.enabledLayerCount       = instanceProperties.enabledLayerCount;
    deviceInfo.ppEnabledLayerNames     = instanceProperties.enabledLayers;
    deviceInfo.enabledExtensionCount   = deviceProperties.enabledExtensionCount;
    deviceInfo.ppEnabledExtensionNames = deviceProperties.enabledExtensions;
    deviceInfo.pEnabledFeatures        = &deviceProperties.physicalDeviceFeatures;

    CHECK_INT(vkCreateDevice(deviceProperties.physicalDevice, &deviceInfo, allocator, &device),
              "Failed to create a virtual graphics device.");

    if (graphicsQueueFamilyIndex != UINT32_MAX)
    {
        vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
    }

    if (computeQueueFamilyIndex != UINT32_MAX)
    {
        vkGetDeviceQueue(device, computeQueueFamilyIndex, 0, &computeQueue);
    }
    else
    {
        // No async compute, fall back to the graphics queue.
        computeQueue = graphicsQueue;
    }

    if (transferQueueFamilyIndex != UINT32_MAX)
    {
        vkGetDeviceQueue(device, transferQueueFamilyIndex, 0, &transferQueue);
    }
    else
    {
        // No async transfers, fall back to the graphics queue.
        transferQueue = graphicsQueue;
    }

    if (presentQueueFamilyIndex == computeQueueFamilyIndex)
    {
        presentQueue = computeQueue;
    }
    else if (presentQueueFamilyIndex == graphicsQueueFamilyIndex)
    {
        presentQueue = graphicsQueue;
    }
    else if (presentQueueFamilyIndex == transferQueueFamilyIndex)
    {
        presentQueue = transferQueue;
    }
    else
    {
        // Use a dedicated presentation queue.
        vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
    }
}

void VulkanRenderBackEnd::DestroyGraphicsDevice()
{
    // TODO: clean up VulkanDeviceProperties.
    vkDestroyDevice(device, allocator);
}

void VulkanRenderBackEnd::CreateSyncPrimitives()
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    CHECK_INT(vkCreateSemaphore(device, &semaphoreInfo, allocator, &semaphore),
              "Failed to create a semaphore.");
}

void VulkanRenderBackEnd::DestroySyncPrimitives()
{
    vkDeviceWaitIdle(device);
    vkDestroySemaphore(device, semaphore, allocator);
}
