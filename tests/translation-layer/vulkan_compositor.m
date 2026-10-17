/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Real MoltenVK swapchains, no window: asymmetric pixels expose channel/row
 * mistakes and separate surfaces expose lifetime and stale-frame errors. */
#import <Foundation/Foundation.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "husk-tl-vulkan.h"
#include "husk-tl-ld.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL Vulkan compositor line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
#define VK(c) CHECK((c) == VK_SUCCESS)
#define WIDTH 17
#define HEIGHT 12
typedef struct { int refs; bool texture; } Window;
static uint8_t observed[WIDTH * HEIGHT * 4];
static unsigned java_frames, texture_frames, clears;
static bool java_compositing = true;
static void acquire(void *p) { ((Window *)p)->refs++; }
static void release(void *p) { ((Window *)p)->refs--; }
void *tl_bionic_find(const char *s) {
    if (!strcmp(s,"ANativeWindow_acquire")) return acquire;
    if (!strcmp(s,"ANativeWindow_release")) return release;
    return NULL;
}
tl_lib *tl_ld_find_lib(const char *s) { (void)s; return NULL; }
void tl_log_line(const char *format, ...) {
    va_list ap; va_start(ap,format); vfprintf(stderr,format,ap); va_end(ap); fputc('\n',stderr);
}
void *tl_nwindow_native(void *p) { (void)p; return NULL; }
int tl_nwindow_width(void *p) { (void)p; return WIDTH; }
int tl_nwindow_height(void *p) { (void)p; return HEIGHT; }
void *tl_nwindow_texture(void *p) { return ((Window *)p)->texture ? p : NULL; }
bool tl_javaapp_compositing(void) { return java_compositing; }
void tl_javaapp_app_pixels(const void *px, int w, int h) {
    if (!px) { clears++; return; }
    CHECK(w == WIDTH && h == HEIGHT); memcpy(observed,px,sizeof(observed)); java_frames++;
}
void tl_nwindow_post(void *p, const void *px, int w, int h, bool flip) {
    CHECK(((Window *)p)->texture && !flip && w == WIDTH && h == HEIGHT);
    memcpy(observed,px,sizeof(observed)); texture_frames++;
}

#define FUNCTIONS(X) \
    X(vkCreateInstance) X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkCreateDevice) X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkDeviceWaitIdle) \
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) X(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) X(vkGetSwapchainImagesKHR) X(vkAcquireNextImageKHR) \
    X(vkDestroySurfaceKHR) X(vkQueuePresentKHR) X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) X(vkResetFences) \
    X(vkCreateSemaphore) X(vkDestroySemaphore) \
    X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) \
    X(vkFreeMemory) X(vkBindBufferMemory) X(vkMapMemory) X(vkUnmapMemory) \
    X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkFreeCommandBuffers) \
    X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCmdPipelineBarrier) X(vkCmdCopyBufferToImage) X(vkQueueSubmit)
#define DECLARE(name) static PFN_##name name;
FUNCTIONS(DECLARE)
typedef struct { VkStructureType sType; const void *pNext; VkFlags flags; void *window; } AndroidSurfaceInfo;
typedef VkResult (*CreateAndroidSurface)(VkInstance, const AndroidSurfaceInfo *, const VkAllocationCallbacks *, VkSurfaceKHR *);
static VkDevice device;
static VkPhysicalDevice physical;
static VkQueue queue, render_queue;
static VkCommandPool pool;

static void present(VkSwapchainKHR swapchain, VkFormat format, bool texture, bool composited, unsigned seed) {
    VkFenceCreateInfo fence_info = {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence; VK(vkCreateFence(device,&fence_info,NULL,&fence));
    uint32_t index; VK(vkAcquireNextImageKHR(device,swapchain,1000000000,VK_NULL_HANDLE,fence,&index));
    VK(vkWaitForFences(device,1,&fence,VK_TRUE,1000000000)); VK(vkResetFences(device,1,&fence));
    uint32_t count = 0; VK(vkGetSwapchainImagesKHR(device,swapchain,&count,NULL)); CHECK(count <= 16 && index < count);
    VkImage images[16]; VK(vkGetSwapchainImagesKHR(device,swapchain,&count,images));
    VkBufferCreateInfo buffer_info = {.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=sizeof(observed),.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    VkBuffer buffer; VK(vkCreateBuffer(device,&buffer_info,NULL,&buffer));
    VkMemoryRequirements requirements; vkGetBufferMemoryRequirements(device,buffer,&requirements);
    VkPhysicalDeviceMemoryProperties memory; vkGetPhysicalDeviceMemoryProperties(physical,&memory);
    unsigned type = 0; VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    while (type < memory.memoryTypeCount && (!(requirements.memoryTypeBits & (1u << type)) || (memory.memoryTypes[type].propertyFlags & flags) != flags)) type++;
    CHECK(type < memory.memoryTypeCount);
    VkMemoryAllocateInfo allocation = {.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=requirements.size,.memoryTypeIndex=type};
    VkDeviceMemory storage; VK(vkAllocateMemory(device,&allocation,NULL,&storage)); VK(vkBindBufferMemory(device,buffer,storage,0));
    uint8_t *bytes; VK(vkMapMemory(device,storage,0,sizeof(observed),0,(void **)&bytes));
    uint8_t expected[sizeof(observed)];
    bool bgra = format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
    for (unsigned y=0;y<HEIGHT;y++) for (unsigned x=0;x<WIDTH;x++) {
        unsigned dst = (y * WIDTH + x) * 4, out = ((texture ? y : HEIGHT-1-y) * WIDTH + x) * 4;
        uint8_t r = 30 + x + seed, g = 70 + y, b = 150 + y + seed;
        bytes[dst] = bgra ? b : r; bytes[dst+1] = g; bytes[dst+2] = bgra ? r : b; bytes[dst+3] = 255;
        expected[out] = r; expected[out+1] = g; expected[out+2] = b; expected[out+3] = 255;
    }
    vkUnmapMemory(device,storage);
    VkCommandBufferAllocateInfo command_info = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkCommandBuffer command; VK(vkAllocateCommandBuffers(device,&command_info,&command));
    VkCommandBufferBeginInfo begin = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; VK(vkBeginCommandBuffer(command,&begin));
    VkImageMemoryBarrier barrier = {.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=images[index],
        .subresourceRange={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.levelCount=1,.layerCount=1}};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    VkBufferImageCopy copy = {.imageSubresource={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.layerCount=1},.imageExtent={WIDTH,HEIGHT,1}};
    vkCmdCopyBufferToImage(command,buffer,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=0;
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);
    VK(vkEndCommandBuffer(command));
    VkSemaphoreCreateInfo semaphore_info = {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore rendered; VK(vkCreateSemaphore(device,&semaphore_info,NULL,&rendered));
    VkSubmitInfo submit = {.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command,
        .signalSemaphoreCount=1,.pSignalSemaphores=&rendered};
    VK(vkQueueSubmit(render_queue,1,&submit,fence));
    unsigned before_java = java_frames, before_texture = texture_frames;
    VkPresentInfoKHR present = {.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.swapchainCount=1,.pSwapchains=&swapchain,.pImageIndices=&index,
        .waitSemaphoreCount=1,.pWaitSemaphores=&rendered};
    VK(vkQueuePresentKHR(queue,&present)); VK(vkDeviceWaitIdle(device));
    CHECK(present.waitSemaphoreCount==1 && present.pWaitSemaphores==&rendered);
    CHECK(java_frames == before_java + (composited && !texture));
    CHECK(texture_frames == before_texture + (composited && texture));
    if (composited) CHECK(!memcmp(expected,observed,sizeof(expected)));
    vkFreeCommandBuffers(device,pool,1,&command); vkDestroyBuffer(device,buffer,NULL); vkFreeMemory(device,storage,NULL);
    vkDestroyFence(device,fence,NULL); vkDestroySemaphore(device,rendered,NULL);
}

int main(int argc, char **argv) { @autoreleasepool {
    if (argc != 2 && !(argc == 3 && !strcmp(argv[2], "--same-queue"))) return 64;
    tl_vk_configure(argv[1],NULL,0);
#define LOAD(name) name = (PFN_##name)tl_vk_resolve(#name); CHECK(name);
    FUNCTIONS(LOAD)
    CreateAndroidSurface create_surface = (CreateAndroidSurface)tl_vk_resolve("vkCreateAndroidSurfaceKHR"); CHECK(create_surface);
    const char *extensions[] = {"VK_KHR_surface","VK_KHR_android_surface"};
    VkApplicationInfo app = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo instance_info = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app,.enabledExtensionCount=2,.ppEnabledExtensionNames=extensions};
    VkInstance instance; VK(vkCreateInstance(&instance_info,NULL,&instance));
    uint32_t count = 1; VK(vkEnumeratePhysicalDevices(instance,&count,&physical)); CHECK(count);
    VkQueueFamilyProperties families[32]; count=32; vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families);
    uint32_t family=0; while (family<count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) family++; CHECK(family<count);
    /* MoltenVK exposes one queue per family. Use another graphics family to
     * exercise the present semaphore wait before Metal reads the rendered image. */
    uint32_t render_family=family;
    if (argc==2) for (unsigned i=0;i<count;i++) if (i!=family && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { render_family=i; break; }
    float priority=1;
    uint32_t queue_count=render_family==family?1:2, family_indices[]={family,render_family};
    printf("Testing with %u Vulkan queue(s), present family %u, render family %u\n", queue_count, family, render_family);
    VkDeviceQueueCreateInfo queue_info[] = {
        {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority},
        {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=render_family,.queueCount=1,.pQueuePriorities=&priority},
    };
    const char *device_extensions[] = {"VK_KHR_portability_subset","VK_KHR_swapchain"};
    VkDeviceCreateInfo device_info = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=queue_count,.pQueueCreateInfos=queue_info,.enabledExtensionCount=2,.ppEnabledExtensionNames=device_extensions};
    VK(vkCreateDevice(physical,&device_info,NULL,&device)); vkGetDeviceQueue(device,family,0,&queue);
    vkGetDeviceQueue(device,render_family,0,&render_queue);
    VkCommandPoolCreateInfo pool_info = {.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.queueFamilyIndex=render_family}; VK(vkCreateCommandPool(device,&pool_info,NULL,&pool));
    for (unsigned pass=0;pass<5;pass++) {
        bool composited = pass < 4;
        java_compositing = composited;
        Window window = {.refs=1,.texture=pass%2};
        @autoreleasepool {
        AndroidSurfaceInfo android = {.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,.window=&window};
        VkSurfaceKHR surface; VK(create_surface(instance,&android,NULL,&surface));
        VkSurfaceCapabilitiesKHR cap; VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&cap));
        count=0; VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,NULL)); CHECK(count && count<=256);
        VkSurfaceFormatKHR formats[256]; VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,formats));
        VkFormat format = pass==2 || pass==3 ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
        unsigned selected=0; while (selected<count && formats[selected].format!=format) selected++; CHECK(selected<count);
        CHECK(cap.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        VkSwapchainCreateInfoKHR info = {.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,.surface=surface,.minImageCount=cap.minImageCount,
            .imageFormat=formats[selected].format,.imageColorSpace=formats[selected].colorSpace,.imageExtent={WIDTH,HEIGHT},.imageArrayLayers=1,
            .imageSharingMode=queue_count==2?VK_SHARING_MODE_CONCURRENT:VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount=queue_count,.pQueueFamilyIndices=family_indices,
            .imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,.preTransform=cap.currentTransform,
            .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,.presentMode=VK_PRESENT_MODE_FIFO_KHR,.clipped=VK_TRUE};
        VkSwapchainKHR first, replacement; VK(vkCreateSwapchainKHR(device,&info,NULL,&first));
        present(first,info.imageFormat,window.texture,composited,pass);
        CHECK(window.refs==1+composited);
        info.oldSwapchain=first; VK(vkCreateSwapchainKHR(device,&info,NULL,&replacement));
        unsigned before_clear=clears; vkDestroySwapchainKHR(device,first,NULL); CHECK(clears==before_clear);
        present(replacement,info.imageFormat,window.texture,composited,pass+40);
        before_clear=clears; vkDestroySwapchainKHR(device,replacement,NULL); CHECK(clears==before_clear+(composited && !window.texture));
        vkDestroySurfaceKHR(instance,surface,NULL);
        }
        CHECK(window.refs==1);
    }
    CHECK(java_frames==4 && texture_frames==4 && clears==2);
    vkDestroyCommandPool(device,pool,NULL); vkDestroyDevice(device,NULL); vkDestroyInstance(instance,NULL);
    puts("PASS real Vulkan Java/TextureView pixels, channels, orientation, present waits, replacement, window lifetime and direct presentation");
    return 0;
} }
