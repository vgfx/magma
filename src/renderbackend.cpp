#include "renderbackend.h"
#include "utility.h"
#include "window.h"

#include <algorithm>
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

VulkanRenderBackEnd::VulkanRenderBackEnd()
{
    // Careful with memset() and VTable.
    byte_t* start = reinterpret_cast<byte_t*>(&allocator);
    byte_t* end   = reinterpret_cast<byte_t*>(this + 1);
    memset(start, 0, static_cast<size_t>(end - start));
}

VulkanInstanceProperties VulkanRenderBackEnd::GetInstanceProperties() const
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
    ip.activeExtensionCount = 0;
    ip.activeExtensions     = new string_t[VK_REQ_INSTANCE_EXTENSIONS + VK_OPT_INSTANCE_EXTENSIONS];

    // Check whether all the required extensions are supported.
    for (string_t extName : requiredExtensions)
    {
        ASSERT(ContainsVulkanExtension(extName, ip.supportedExtensions, ip.supportedExtensionCount),
               "The required extension \'%s\' is not supported by the graphics API.", extName);

        ip.activeExtensions[ip.activeExtensionCount++] = extName;
    }

    // Check whether any of the optional extensions are supported.
    for (string_t extName : optionalExtensions)
    {
        if (ContainsVulkanExtension(extName, ip.supportedExtensions, ip.supportedExtensionCount))
        {
            ip.activeExtensions[ip.activeExtensionCount++] = extName;
        }
    }

    return ip;
}

void VulkanRenderBackEnd::CreateApiInstance()
{
    this->instanceProperties = GetInstanceProperties();

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
    instanceInfo.enabledExtensionCount   = instanceProperties.activeExtensionCount;
    instanceInfo.ppEnabledExtensionNames = instanceProperties.activeExtensions;

    CHECK_INT(vkCreateInstance(&instanceInfo, allocator, &instance),
              "Failed to create a graphics API instance.");
}

void VulkanRenderBackEnd::DestroyApiInstance()
{
    // TODO: clean up VulkanInstanceProperties.
    vkDestroyInstance(instance, allocator);
}

VkResult vkCreateSurfaceKHR(VkInstance instance, const void* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
#ifdef WIN32
    return vkCreateWin32SurfaceKHR(instance, static_cast<const VkWin32SurfaceCreateInfoKHR*>(pCreateInfo), pAllocator, pSurface);  
#endif
}

void VulkanRenderBackEnd::CreateDisplaySurface(const Window& window)
{
    surfaceDimensions.width  = window.Width();
    surfaceDimensions.height = window.Height();

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

VulkanDeviceProperties VulkanRenderBackEnd::GetDeviceProperties() const
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
        for (uint32_t f = 0; f < queueFamilyCount; f++)
        {
            VkBool32 queueCanPresent;
            CHECK_INT(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, f, surface, &queueCanPresent),
                      "Failed to query the graphics device surface support.");

            if (queueCanPresent)
            {
                queueFamilies[f].queueFlags |= VK_QUEUE_PRESENT_BIT;
            }

            VkQueueFlags queueFlags = queueFamilies[f].queueFlags;

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
            // Prevent memory leaks in case we enter this block several times.
            delete[] dp.supportedExtensions;
            delete[] dp.activeExtensions;
            delete[] dp.queueFamilies;

            dp.physicalDevice           = physicalDevice;
            dp.physicalDeviceProperties = physicalDeviceProperties;
            dp.physicalDeviceFeatures   = physicalDeviceFeatures;

            dp.supportedExtensionCount  = supportedExtensionCount;
            dp.supportedExtensions      = supportedExtensions.release();

            dp.activeExtensionCount     = 0;
            dp.activeExtensions         = new string_t[VK_REQ_DEVICE_EXTENSIONS + VK_OPT_DEVICE_EXTENSIONS];

            // Copy all of the required extensions. At this point, we know that all of them are supported.
            for (string_t extName : requiredExtensions)
            {
                dp.activeExtensions[dp.activeExtensionCount++] = extName;
            }

            // Check whether any of the optional extensions are supported.
            for (string_t extName : optionalExtensions)
            {
                if (ContainsVulkanExtension(extName, dp.supportedExtensions, dp.supportedExtensionCount))
                {
                    dp.activeExtensions[dp.activeExtensionCount++] = extName;
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
    this->deviceProperties = GetDeviceProperties();

    // The queue family index is overwritten only if a dedicated queue is available
    // (except for the present queue which is always assigned).
    uint32_t graphicsQueueIndex = 0, graphicsQueueFamilyIndex = UINT32_MAX,
             computeQueueIndex  = 0, computeQueueFamilyIndex  = UINT32_MAX,
             transferQueueIndex = 0, transferQueueFamilyIndex = UINT32_MAX,
             presentQueueIndex  = 0, presentQueueFamilyIndex  = UINT32_MAX;

    for (uint32_t f = 0; f < deviceProperties.queueFamilyCount; f++)
    {
        uint32_t i = 0, n = deviceProperties.queueFamilies[f].queueCount;

        VkQueueFlags queueFlags = deviceProperties.queueFamilies[f].queueFlags;

        if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsQueueFamilyIndex = f;
            graphicsQueueIndex       = i++;

            // Use a dedicated compute queue, if possible.
            if ((queueFlags & VK_QUEUE_COMPUTE_BIT) && (computeQueueFamilyIndex == UINT32_MAX) && (i < n))
            {
                computeQueueFamilyIndex = f;
                computeQueueIndex       = i++;
            }

            // Use a dedicated transfer queue, if possible.
            if ((queueFlags & VK_QUEUE_TRANSFER_BIT) && (transferQueueFamilyIndex == UINT32_MAX) && (i < n))
            {
                transferQueueFamilyIndex = f;
                transferQueueIndex       = i++;
            }

            // Use the compute queue to present, if possible.
            if ((queueFlags & VK_QUEUE_PRESENT_BIT) && (presentQueueFamilyIndex == UINT32_MAX ||
                                                        presentQueueFamilyIndex != computeQueueFamilyIndex))
            {
                presentQueueFamilyIndex = f;
                presentQueueIndex       = (queueFlags & VK_QUEUE_COMPUTE_BIT) ? computeQueueIndex
                                                                              : graphicsQueueIndex;
            }
        }
        else if (queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            computeQueueFamilyIndex = f;
            computeQueueIndex       = i++;

            // Use a dedicated transfer queue, if possible.
            if ((queueFlags & VK_QUEUE_TRANSFER_BIT) && (transferQueueFamilyIndex == UINT32_MAX) && (i < n))
            {
                transferQueueFamilyIndex = f;
                transferQueueIndex       = i++;
            }

            // Use the compute queue to present, if possible.
            if (queueFlags & VK_QUEUE_PRESENT_BIT)
            {
                presentQueueFamilyIndex = f;
                presentQueueIndex       = computeQueueIndex;
            }
        }
        else if (queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            transferQueueFamilyIndex = f;
            transferQueueIndex       = 0;

            // Avoid presenting from the transfer queue, if possible.
            if ((queueFlags & VK_QUEUE_PRESENT_BIT) && (presentQueueFamilyIndex == UINT32_MAX))
            {
                presentQueueFamilyIndex = f;
                presentQueueIndex       = 0;
            }
        }
        else if ((queueFlags & VK_QUEUE_PRESENT_BIT) && (presentQueueFamilyIndex == UINT32_MAX))
        {
            // Use a dedicated presentation queue as the last resort.
            presentQueueFamilyIndex = f;
            presentQueueIndex       = 0;
        }
    }

    uint32_t                queueInfoCount = 0;
    VkDeviceQueueCreateInfo queueInfos[VK_MAX_QUEUE_FAMILIES] = {};

    // TODO: prioritize.
    constexpr float priorities[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    if (graphicsQueueFamilyIndex != UINT32_MAX)
    {
        uint32_t count = 1;

        if (graphicsQueueFamilyIndex == computeQueueFamilyIndex)  count++;
        if (graphicsQueueFamilyIndex == transferQueueFamilyIndex) count++;

        queueInfos[queueInfoCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueInfoCount].queueFamilyIndex = graphicsQueueFamilyIndex;
        queueInfos[queueInfoCount].queueCount       = count;
        queueInfos[queueInfoCount].pQueuePriorities = priorities;        

        queueInfoCount++;
    }

    if (computeQueueFamilyIndex != UINT32_MAX &&
        computeQueueFamilyIndex != graphicsQueueFamilyIndex)
    {
        uint32_t count = 1;

        if (computeQueueFamilyIndex == transferQueueFamilyIndex) count++;

        queueInfos[queueInfoCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueInfoCount].queueFamilyIndex = computeQueueFamilyIndex;
        queueInfos[queueInfoCount].queueCount       = count;
        queueInfos[queueInfoCount].pQueuePriorities = priorities;        

        queueInfoCount++;
    }

    if (transferQueueFamilyIndex != UINT32_MAX &&
        transferQueueFamilyIndex != graphicsQueueFamilyIndex && 
        transferQueueFamilyIndex != computeQueueFamilyIndex)
    {
        queueInfos[queueInfoCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueInfoCount].queueFamilyIndex = transferQueueFamilyIndex;
        queueInfos[queueInfoCount].queueCount       = 1;
        queueInfos[queueInfoCount].pQueuePriorities = priorities;        

        queueInfoCount++;
    }

    if (presentQueueFamilyIndex != UINT32_MAX &&
        presentQueueFamilyIndex != graphicsQueueFamilyIndex &&
        presentQueueFamilyIndex != computeQueueFamilyIndex &&
        presentQueueFamilyIndex != transferQueueFamilyIndex)
    {
        queueInfos[queueInfoCount].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueInfoCount].queueFamilyIndex = presentQueueFamilyIndex;
        queueInfos[queueInfoCount].queueCount       = 1;
        queueInfos[queueInfoCount].pQueuePriorities = priorities;        

        queueInfoCount++;
    }

    // Create a virtual device. Enable all supported features.
    VkDeviceCreateInfo deviceInfo      = {};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount    = queueInfoCount;
    deviceInfo.pQueueCreateInfos       = queueInfos;
    deviceInfo.enabledLayerCount       = instanceProperties.enabledLayerCount;
    deviceInfo.ppEnabledLayerNames     = instanceProperties.enabledLayers;
    deviceInfo.enabledExtensionCount   = deviceProperties.activeExtensionCount;
    deviceInfo.ppEnabledExtensionNames = deviceProperties.activeExtensions;
    deviceInfo.pEnabledFeatures        = &deviceProperties.physicalDeviceFeatures;

    CHECK_INT(vkCreateDevice(deviceProperties.physicalDevice, &deviceInfo, allocator, &device),
              "Failed to create a virtual graphics device.");

    if (computeQueueFamilyIndex == UINT32_MAX)
    {
        // No async compute, fall back to the graphics queue.
        computeQueueFamilyIndex = graphicsQueueFamilyIndex;
        computeQueueIndex       = graphicsQueueIndex;
    }

    if (transferQueueFamilyIndex == UINT32_MAX)
    {
        // No async transfers, fall back to the graphics queue.
        transferQueueFamilyIndex = graphicsQueueFamilyIndex;
        transferQueueFamilyIndex = graphicsQueueIndex;
    }

    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, graphicsQueueIndex, &graphicsQueue);
    vkGetDeviceQueue(device, computeQueueFamilyIndex,  computeQueueIndex,  &computeQueue);
    vkGetDeviceQueue(device, transferQueueFamilyIndex, transferQueueIndex, &transferQueue);
    vkGetDeviceQueue(device, presentQueueFamilyIndex,  presentQueueIndex,  &presentQueue);
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

VulkanSwapChainProperties VulkanRenderBackEnd::GetSwapChainProperties() const
{
    VulkanSwapChainProperties sp = {};

    CHECK_INT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deviceProperties.physicalDevice, surface, &sp.surfaceCapabilities),
              "Failed to query surface capabilities of the graphics device.");

    CHECK_INT(vkGetPhysicalDeviceSurfaceFormatsKHR(deviceProperties.physicalDevice, surface, &sp.surfaceFormatCount, nullptr),
              "Failed to enumerate surface formats supported by the graphics device.");

    sp.surfaceFormats = new VkSurfaceFormatKHR[sp.surfaceFormatCount];
    CHECK_INT(vkGetPhysicalDeviceSurfaceFormatsKHR(deviceProperties.physicalDevice, surface, &sp.surfaceFormatCount, sp.surfaceFormats),
              "Failed to enumerate surface formats supported by the graphics device.");

    CHECK_INT(vkGetPhysicalDeviceSurfacePresentModesKHR(deviceProperties.physicalDevice, surface, &sp.presentModeCount, nullptr),
              "Failed to enumerate presentation modes supported by the graphics device.");

    sp.presentModes = new VkPresentModeKHR[sp.presentModeCount];
    CHECK_INT(vkGetPhysicalDeviceSurfacePresentModesKHR(deviceProperties.physicalDevice, surface, &sp.presentModeCount, sp.presentModes),
              "Failed to enumerate presentation modes supported by the graphics device.");

    // Pick a surface format. Prefer sRGB.
    // TODO: HDR, WCG.
    sp.activeSurfaceFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    for (uint32_t i = 0; i < sp.surfaceFormatCount; i++)
    {
        if (sp.surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            sp.activeSurfaceFormat = sp.surfaceFormats[i];
        }
    }
    
    // These two flags allow color writes and clears.
    sp.activeSurfaceUsageFlags  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sp.activeSurfaceUsageFlags &= sp.surfaceCapabilities.supportedUsageFlags;
    
    // TODO: rotation and scaling.
    sp.activeSurfaceTransforms  = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    // Select the presentation mode.
    // VK_PRESENT_MODE_FIFO_KHR is always available, but VK_PRESENT_MODE_MAILBOX_KHR results in lower latency.
    // Neither mode allows tearing.
    sp.activePresentMode = VK_PRESENT_MODE_FIFO_KHR;

    for (uint32_t i = 0; i < sp.presentModeCount; i++)
    {
        if (sp.presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            sp.activePresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    return sp;
}

void VulkanRenderBackEnd::CreateSwapChain()
{
    this->swapChainProperties = GetSwapChainProperties();

    // Triple buffering is highly desirable for max performance.
    // It is also required for the VK_PRESENT_MODE_MAILBOX_KHR mode.
    // https://software.intel.com/en-us/articles/sample-application-for-direct3d-12-flip-model-swap-chains
    bufferCount = 3;
    bufferCount = std::max(bufferCount, swapChainProperties.surfaceCapabilities.minImageCount);
    bufferCount = std::min(bufferCount, swapChainProperties.surfaceCapabilities.maxImageCount);

    // Adjust the resolution if needed.
    if (surfaceDimensions.width  != swapChainProperties.surfaceCapabilities.currentExtent.width ||
        surfaceDimensions.height != swapChainProperties.surfaceCapabilities.currentExtent.height)
    {
        surfaceDimensions.width  = std::max(surfaceDimensions.width,  swapChainProperties.surfaceCapabilities.minImageExtent.width);
        surfaceDimensions.width  = std::min(surfaceDimensions.width,  swapChainProperties.surfaceCapabilities.maxImageExtent.width);
        surfaceDimensions.height = std::max(surfaceDimensions.height, swapChainProperties.surfaceCapabilities.minImageExtent.height);
        surfaceDimensions.height = std::min(surfaceDimensions.height, swapChainProperties.surfaceCapabilities.maxImageExtent.height);
    }

    VkSwapchainCreateInfoKHR swapChainInfo = {};
    swapChainInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface          = surface;
    swapChainInfo.minImageCount    = bufferCount;
    swapChainInfo.imageFormat      = swapChainProperties.activeSurfaceFormat.format;
    swapChainInfo.imageColorSpace  = swapChainProperties.activeSurfaceFormat.colorSpace;
    swapChainInfo.imageExtent      = surfaceDimensions;
    swapChainInfo.imageArrayLayers = 1;         // Only for stereo rendering
    swapChainInfo.imageUsage       = swapChainProperties.activeSurfaceUsageFlags;
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainInfo.preTransform     = swapChainProperties.activeSurfaceTransforms;
    swapChainInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainInfo.presentMode      = swapChainProperties.activePresentMode;
    swapChainInfo.clipped          = VK_TRUE;   // Skip rendering of fragments which are not visible
    swapChainInfo.oldSwapchain     = swapChain; // In case we want to re-create it

    CHECK_INT(vkCreateSwapchainKHR(device, &swapChainInfo, allocator, &swapChain),
              "Failed to create a swap chain.");
}

void VulkanRenderBackEnd::DestroySwapChain()
{
    vkDestroySwapchainKHR(device, swapChain, allocator);
}
