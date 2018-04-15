#include "renderbackend.h"
#include "utility.h"
#include "window.h"

class Renderer
{
public:
    RenderBackEnd* renderBackEnd;
};

Renderer renderer;

int main(const int argc, string_t argv[])
{
    ASSERT(argc == 3, "Missing command line arguments: resolution. E.g.: 1920 1080.");

    uint16_t windowWidth  = static_cast<uint16_t>(atoi(argv[1]));
    uint16_t windowHeight = static_cast<uint16_t>(atoi(argv[2]));

    // Create the OS window used for drawing. Needed to create the RBE display surface. 
    Window window = Window(windowWidth, windowHeight);

    renderer.renderBackEnd = new VulkanRenderBackEnd();

    renderer.renderBackEnd->CreateApiInstance();
    renderer.renderBackEnd->CreateDisplaySurface(window);
    renderer.renderBackEnd->CreateGraphicsDevice();
    renderer.renderBackEnd->CreateSyncPrimitives();

    //uint32_t surfaceFormatCount;
    //CHECK_INT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr),
    //          "Failed to enumerate surface formats supported by the graphics device.");

    //auto surfaceFormats = std::make_unique<VkSurfaceFormatKHR[]>(surfaceFormatCount);
    //CHECK_INT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats),
    //          "Failed to enumerate  surface formats supported by the graphics device.");

    //VkSurfaceCapabilitiesKHR surfaceCapabilities;
    //CHECK_INT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities),
    //          "Failed to query surface capabilities of the graphics device.");

    // Clean up.
    // API note: you only have to vkDestroy() objects you vkCreate().

    renderer.renderBackEnd->DestroySyncPrimitives();
    renderer.renderBackEnd->DestroyGraphicsDevice();
    renderer.renderBackEnd->DestroyDisplaySurface();
    renderer.renderBackEnd->DestroyApiInstance();

    delete renderer.renderBackEnd;

    return EXIT_SUCCESS;
}
