#ifndef VKTERM_PRIVATE_H
#define VKTERM_PRIVATE_H

#include <stdint.h>

#define VMA_DEBUG_LOG
#include <vulkan/vulkan.h>

#include "../extern/vk_mem_alloc.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/* Designed for synchronous gpu/cpu operation
 * as its meant to have lower input delay and less vram use 
 * need double vertex buffers for 2 */
#define FRAMES_IN_FLIGHT 1

/* Asset Loading Options */
#define COLOR_TEXTURE_INDEX 0
#define PALETTE_SIZE 16
#define ASCII_TEXTURE_INDEX 1
#define DRAW_TEXTURE_INDEX 2
#define ATLAS_WIDTH 32
/* TO REMOVE */
#define ASCII_TILE_SIZE 8
//#define ASCII_SCALE 2.0f

#define cfg_format (VkFormat) VK_FORMAT_B8G8R8A8_SRGB

#define MAX_TILESETS 4

#define MAX_LAYERS 6
#define MAX_SPRITES 16

#define DEBUG_BUFFER 0
#define DEBUG_LAYERS 1

typedef void(VKAPI_PTR* PFN_vkCmdBeginRenderingKHR)(VkCommandBuffer,
                                                    const VkRenderingInfo*);
extern PFN_vkCmdBeginRenderingKHR pfn_vkCmdBeginRenderingKHR;
extern PFN_vkCmdEndRenderingKHR pfn_vkCmdEndRenderingKHR;

#define VK_CHECK(err_) do {                                                  \
    if (err_) {                                                              \
      fprintf(stderr, "Error: %s returned %d (line %d in %s)\n", #err_,      \
              (err_), __LINE__, __FILE__);                                   \
      abort();                                                               \
    }                                                                        \
  } while (0)

struct GpuBuffer {
  VkBuffer handle;
  VmaAllocation allocation;
  VkDeviceSize top;
};

struct GpuImage {
  VkImage handle;
  VmaAllocation allocation;
  VkImageView view;
  VkSampler sampler;
};

struct GpuContext {
  /* const */ VkInstance instance;
  /* const */ VkPhysicalDevice pdev;
  /* const */ VkDevice ldev;
  /* const */ VkQueue queue;
  /* const */ VmaAllocator allocator;
  /* const */ VkCommandPool cmd_pool;
  /* const */ VkSurfaceKHR surface;
  /* const */ VkExtent2D extent;

  /* Swapchain */
  /* const */ VkSwapchainKHR swapchain;
  /* const */ uint32_t swapchain_c;
  uint32_t swapchain_x;
  VkImage* swapchain_images;
  VkImageView* swapchain_views;
  /* const */ uint32_t frame_c;
  uint32_t frame_x;

  /* Aux */
  /* const */ VkDescriptorPool descriptor_pool;
  /* const */ VkCommandBuffer* cmd_buffer;
  /* const */ VkSemaphore* image_available;
  /* const */ VkSemaphore* render_finished;
  /* const */ VkFence* fence;

  /* Bios Pipeline */
  VkDescriptorSetLayout texture_descriptors_layout;
  VkDescriptorSet texture_descriptors;
  VkDescriptorSetLayout frame_descriptors_layout;
  VkDescriptorSet frame_descriptors;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
};

typedef uint32_t (*DecoderFunc)(uint32_t* encoding, uint32_t count,
                                uint32_t unicode);
struct TermTileset {
  struct GpuImage image;
  uint32_t image_h;
  uint32_t image_w;
  uint32_t channels;

  uint32_t* encodings;
  DecoderFunc decoder;
  uint32_t glyph_c;
  uint32_t glyph_h;
  uint32_t glyph_w;
};

/* INIT */
int gpuDevicesCreate(struct GpuContext*, GLFWwindow*);
int gpuSwapchainCreate(struct GpuContext*, uint32_t, uint32_t);
int gpuSwapchainRecreate(struct GpuContext*, uint32_t, uint32_t);
int gpuAuxiliaryCreate(struct GpuContext* gpu);
int gpuTextureDescriptorsCreate(struct GpuContext*);
int gpuPipelineCreate(struct GpuContext* gpu);
int gpuSpvLoad(VkDevice l_dev, const char* filename, VkShaderModule* shader);

/* Command Buffer Singleshots */
VkCommandBuffer gpuCmdSingleBegin(struct GpuContext*);
int gpuCmdSingleEnd(struct GpuContext*, VkCommandBuffer);

/* Memory Buffers */
int gpuBufferCreate(struct GpuContext*, VkBufferUsageFlags, VkDeviceSize,
                    struct GpuBuffer*);
int gpuBufferDestroy(VmaAllocator, struct GpuBuffer*);
int gpuBufferPush(struct GpuContext*, struct GpuBuffer*, const void*, VkDeviceSize);
int gpuBufferPop(struct GpuBuffer* dest, size_t nmemb, size_t stride);
void* gpuBufferGetPtr(struct GpuContext*, struct GpuBuffer buf);
size_t gpuBufferCapacity(VmaAllocator, struct GpuBuffer);
struct GpuBuffer* gpuBufferNext(VmaAllocator, struct GpuBuffer*);

/* Textures */
int transitionImageLayout(VkCommandBuffer, VkImage, VkImageLayout, VkImageLayout);
int gpuImageToGpu(struct GpuContext*, unsigned char*, int, int, int, struct GpuImage*);
void gpuImageDestroy(struct GpuContext*, struct GpuImage);
int gpuTexturesDescriptorsUpdate(struct GpuContext*, struct TermTileset*, uint32_t);


#endif  // VULKAN_META_H
