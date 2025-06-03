#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vulkan_backend.h"


PFN_vkCmdBeginRenderingKHR pfn_vkCmdBeginRenderingKHR;
PFN_vkCmdEndRenderingKHR pfn_vkCmdEndRenderingKHR;

VkResult gpuExtFunctionsInit(VkDevice device) {
  pfn_vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(
      device, "vkCmdBeginRenderingKHR");
  if (pfn_vkCmdBeginRenderingKHR == NULL) {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  pfn_vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(
      device, "vkCmdEndRenderingKHR");
  if (pfn_vkCmdBeginRenderingKHR == NULL) {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  return VK_SUCCESS;
}

int gpuSpvLoad(VkDevice l_dev, const char* filename, VkShaderModule* shader) {
  FILE* file = fopen(filename, "rb");
  if (file == NULL) {
    printf("%s not found!", filename);
    return 1;
  }
  if (fseek(file, 0l, SEEK_END) != 0) {
    printf("failed to seek to end of file!");
    return 1;
  }
  size_t length = ftell(file);
  if (length == 0) {
    printf("failed to get file size!");
    return 1;
  }

  char* spv_code = (char*)malloc(length * sizeof(char));
  rewind(file);
  fread(spv_code, length, 1, file);

  VkShaderModuleCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = length,
      .pCode = (const uint32_t*)spv_code};

  if (vkCreateShaderModule(l_dev, &create_info, NULL, shader) != VK_SUCCESS) {
    return 1;
  }

  fclose(file);
  free(spv_code);

  return 0;
}

VkCommandBuffer gpuCmdSingleBegin(struct GpuContext* gpu) {
  VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandPool = gpu->cmd_pool,
      .commandBufferCount = 1,
  };

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(gpu->ldev, &alloc_info, &command_buffer);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(command_buffer, &begin_info);

  return command_buffer;
}

int gpuCmdSingleEnd(struct GpuContext* gpu, VkCommandBuffer cmd_buffer) {
  vkEndCommandBuffer(cmd_buffer);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd_buffer,
  };

  if (vkQueueSubmit(gpu->queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
    return 1;

  vkQueueWaitIdle(gpu->queue);
  vkFreeCommandBuffers(gpu->ldev, gpu->cmd_pool, 1, &cmd_buffer);
  return 0;
}

int gpuBufferPush(struct GpuContext* gpu, struct GpuBuffer* dest,
                  const void* src, VkDeviceSize src_size) {
  VmaAllocationInfo dest_info;
  vmaGetAllocationInfo(gpu->allocator, dest->allocation, &dest_info);

  // printf("buffer append size %llu\n", src_size);

  if (dest->top + src_size > dest_info.size) {
    // fprintf(stderr, "gpuBufferAppend fail\n");
    abort();
  }

  vmaCopyMemoryToAllocation(gpu->allocator, src, dest->allocation, dest->top,
                            src_size);
  dest->top += src_size;
  return 0;
}

int gpuBufferPop(struct GpuBuffer* dest, size_t nmemb, size_t stride) {
  size_t size = nmemb * stride;
  // printf("popping size %zu from %llu\n ", size, dest->top);
  if (size > dest->top) abort();

  dest->top -= size;
  // printf("size after pop %llu\n", dest->top);
  return 0;
}

void* gpuBufferGetPtr(struct GpuContext* gpu, struct GpuBuffer buf){
 
  VmaAllocationInfo allocInfo;
  vmaGetAllocationInfo(gpu->allocator, buf.allocation, &allocInfo);
  return allocInfo.pMappedData;
}

size_t gpuBufferCapacity(VmaAllocator allocator, struct GpuBuffer buffer) {
  VmaAllocationInfo info;
  vmaGetAllocationInfo(allocator, buffer.allocation, &info);
  return info.size;
}

int gpuBufferCreate(struct GpuContext* gpu, VkBufferUsageFlags usage,
                    VkDeviceSize size, struct GpuBuffer* dest) {
  if (DEBUG_BUFFER) {
    printf("+creating buffer %p\n", (void*)dest);
  }

  VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage
  };
  VmaAllocationCreateInfo vma_alloc_info = {
      .usage = VMA_MEMORY_USAGE_AUTO,
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
  };

  VK_CHECK(vmaCreateBuffer(gpu->allocator, &buffer_info, &vma_alloc_info,
                           &dest->handle, &dest->allocation, NULL));
  dest->top = 0;

  return 0;
}

int gpuBufferDestroy(VmaAllocator allocator, struct GpuBuffer* b) {
  if (allocator == NULL || b == NULL) {
    // Log error or handle appropriately
    return 1;
  }

  vmaDestroyBuffer(allocator, b->handle, b->allocation);
  return 0;
}

int gpuImageAlloc(VmaAllocator allocator, struct GpuImage* image,
                  VkImageUsageFlags usage, VkFormat format, uint32_t width,
                  uint32_t height) {
  /* Allocate VkImage memory */
  VkImageCreateInfo image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .extent.width = width,
      .extent.height = height,
      .extent.depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = format,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      //.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .samples = VK_SAMPLE_COUNT_1_BIT,
  };

  VmaAllocationCreateInfo alloc_create_info = {
      .usage = VMA_MEMORY_USAGE_AUTO,
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
      .priority = 1.0f,
  };

  VK_CHECK(vmaCreateImage(allocator, &image_info, &alloc_create_info,
                          &image->handle, &image->allocation, NULL));
  return 0;
}

int gpuImageViewCreate(VkDevice l_dev, VkImage image, VkImageView* view,
                       VkFormat format, VkImageAspectFlags aspect_flags) {
  VkImageViewCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
      .subresourceRange.aspectMask = aspect_flags,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1,
  };
  VK_CHECK(vkCreateImageView(l_dev, &create_info, NULL, view));
  return 0;
}

int gpuCopyBufferToImage(struct GpuContext* gpu, VkBuffer buffer,
        VkImage image, uint32_t width, uint32_t height) {
  VkCommandBuffer command = gpuCmdSingleBegin(gpu);

  VkBufferImageCopy region = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,

      .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .imageSubresource.mipLevel = 0,
      .imageSubresource.baseArrayLayer = 0,
      .imageSubresource.layerCount = 1,

      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1},
  };

  vkCmdCopyBufferToImage(command, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  return gpuCmdSingleEnd(gpu, command);
}

int transitionImageLayout(VkCommandBuffer commands, VkImage image,
                          VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1,
      .srcAccessMask = 0,
      .dstAccessMask = 0,
  };

  VkPipelineStageFlags source_stage, destination_stage;

  int supported_transition = 0;
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    supported_transition = 1;
  }
  if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
      new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    supported_transition = 1;
  }

  if (new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    supported_transition = 1;
  }
  if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destination_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    supported_transition = 1;
  }

  if (supported_transition == 0) {
    printf("unsupported layout transition\n");
    return 1;
  }
  vkCmdPipelineBarrier(commands, source_stage, destination_stage, 0, 0, NULL, 0,
                       NULL, 1, &barrier);
  return 0;
}

void gpuImageDestroy(struct GpuContext* gpu, struct GpuImage image) {
  VmaAllocatorInfo alloc_info;
  vmaGetAllocatorInfo(gpu->allocator, &alloc_info);

  vkDestroySampler(alloc_info.device, image.sampler, NULL);
  vkDestroyImageView(alloc_info.device, image.view, NULL);
  vmaDestroyImage(gpu->allocator, image.handle, image.allocation);
}

int gpuQueueIndex(struct GpuContext* gpu) {
  uint32_t q_fam_c;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu->pdev, &q_fam_c, NULL);
  VkQueueFamilyProperties q_fam[q_fam_c];
  vkGetPhysicalDeviceQueueFamilyProperties(gpu->pdev, &q_fam_c, q_fam);

  uint32_t queue_index = -1;
  for (uint32_t i = 0; i < q_fam_c; i++) {
    if (q_fam[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      queue_index = i;
      break;
    }
  }
  if (queue_index < 0) {
    printf("FATAL valid queue family not found\n");
  }

  return queue_index;
}

int gpuDevicesCreate(struct GpuContext* gpu, GLFWwindow* glfw_window) {
  
  uint32_t vk_version;
  vkEnumerateInstanceVersion(&vk_version);

  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Cars",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Naomi Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = vk_version,  // VK_API_VERSION_1_3,
  };

  uint32_t ext_c;
  const char** ext = glfwGetRequiredInstanceExtensions(&ext_c);

  uint32_t vk_ext_c = 0;
  vkEnumerateInstanceExtensionProperties(NULL, &vk_ext_c, NULL);
  VkExtensionProperties vk_ext[vk_ext_c + 1];
  vkEnumerateInstanceExtensionProperties(NULL, &vk_ext_c, vk_ext);

  int ext_found;
  for (uint32_t g = 0; g < ext_c; g++) {
    ext_found = 0;
    for (uint32_t i = 0; i < vk_ext_c; i++) {
      if (strcmp(ext[g], vk_ext[i].extensionName) == 0) {
        ext_found = 1;
      }
    }
    if (!ext_found) {
      printf("!extension not found! %s\n", ext[g]);
      return 1;
    }
  }

  VkInstanceCreateInfo instance_create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = ext_c,
      .ppEnabledExtensionNames = ext,
      .enabledLayerCount = 0,
  };

  /* Enable validation_layers debugging */
  instance_create_info.enabledLayerCount = 0;
  if (DEBUG_LAYERS != 0) {
    const char* explicit_layers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t debug_layer_c;
    vkEnumerateInstanceLayerProperties(&debug_layer_c, NULL);
    VkLayerProperties debug_layers[debug_layer_c];
    vkEnumerateInstanceLayerProperties(&debug_layer_c, debug_layers);

    instance_create_info.enabledLayerCount = 1;
    instance_create_info.ppEnabledLayerNames = explicit_layers;
  }

  VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &gpu->instance));

  uint32_t dev_c = 0;
  vkEnumeratePhysicalDevices(gpu->instance, &dev_c, NULL);
  if (dev_c == 0) {
    printf("FATAL no devices found\n");
    return 1;
  }
  if (dev_c > 1) {
    printf(
        "Warning: multiple graphics devices found, dont have the logic to "
        "choose\n");
  }
  VkPhysicalDevice devs[dev_c];
  vkEnumeratePhysicalDevices(gpu->instance, &dev_c, devs);
  for (uint32_t i = 0; i < dev_c; i++) {
    VkPhysicalDeviceProperties dev_properties;
    vkGetPhysicalDeviceProperties(devs[i], &dev_properties);
    printf("DEBUG %s\n", dev_properties.deviceName);
  }

  gpu->pdev = devs[0];

  VkPhysicalDeviceFeatures dev_features;
  vkGetPhysicalDeviceFeatures(gpu->pdev, &dev_features);
  if (!dev_features.geometryShader) {
    printf("FATAL: driver missing geometry shader\n");
    return 1;
  }
  if (!dev_features.samplerAnisotropy) {
    printf("FATAL: driver missing sampler anisotropy\n");
    return 1;
  }
  if (!dev_features.multiDrawIndirect) {
    printf("FATAL: driving missing indirect drawing\n");
    return 1;
  }

  /* Requested Extensions */
  const char* dev_ext_names[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    "VK_EXT_descriptor_indexing"};
  const unsigned int dev_ext_c = 4;

  /* Available Extensions */
  uint32_t avail_ext_c;
  vkEnumerateDeviceExtensionProperties(gpu->pdev, NULL, &avail_ext_c, NULL);
  VkExtensionProperties avail_ext[avail_ext_c];
  vkEnumerateDeviceExtensionProperties(gpu->pdev, NULL, &avail_ext_c,
                                       avail_ext);

  /* Check Extension Availability */
  int layer_found;
  for (uint32_t i = 0; i < dev_ext_c; i++) {
    layer_found = 0;
    for (uint32_t a = 0; a < avail_ext_c; a++) {
      if (strcmp(dev_ext_names[i], avail_ext[a].extensionName) == 0) {
        layer_found = 1;
      }
    }
    if (!layer_found) {
      printf("FATAL: driver missing %s\n", dev_ext_names[i]);
      return 1;
    }
  }

  int queue_index = gpuQueueIndex(gpu);
  float priority = 1.0f;
  VkDeviceQueueCreateInfo q_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = queue_index,
      .queueCount = 1,
      .pQueuePriorities = &priority,
  };

  /* Enable Descriptor Indexing */
  VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features = {
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
  };
 
  VkPhysicalDeviceShaderDrawParametersFeatures draw_params_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
    .shaderDrawParameters = VK_TRUE,  // Enable shader draw parameters
    .pNext = &descriptor_indexing_features,
  };

  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
      .dynamicRendering = VK_TRUE,
      .pNext = &draw_params_features};

  VkPhysicalDeviceFeatures2 dev_features2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &dynamic_rendering_feature,
  };

  vkGetPhysicalDeviceFeatures2(gpu->pdev, &dev_features2);

  if (!descriptor_indexing_features.descriptorBindingPartiallyBound) {
    printf("FATAL: driver missing 'descriptor Binding Partially Bound'\n");
    exit(1);
  }
  if (!descriptor_indexing_features.runtimeDescriptorArray) {
    printf("FATAL: driver missing 'runtime descriptor array'\n");
    exit(1);
  }
  if (!dynamic_rendering_feature.dynamicRendering) {
    printf("FATAL: driver missing 'dynamic rendering'\n");
    exit(1);
  }
  VkDeviceCreateInfo dev_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pQueueCreateInfos = &q_create_info,
      .queueCreateInfoCount = 1,
      //.pEnabledFeatures = &dev_features,
      .enabledExtensionCount = dev_ext_c,
      .ppEnabledExtensionNames = dev_ext_names,
      .enabledLayerCount = 0,
      .pNext = &dev_features2,
  };

  if (vkCreateDevice(gpu->pdev, &dev_create_info, NULL, &gpu->ldev) !=
      VK_SUCCESS) {
    printf("FATAL failed to create logical device!\n");
    return 2;
  }

  vkGetDeviceQueue(gpu->ldev, queue_index, 0, &gpu->queue);

  if (glfwCreateWindowSurface(gpu->instance, glfw_window, NULL, &gpu->surface)) {
    printf("failed to create glfw surface");
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
    abort();
  }

  /* Test for queue presentation support */
  VkBool32 present_support = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(gpu->pdev, gpuQueueIndex(gpu),
                                       gpu->surface, &present_support);
  if (present_support == VK_FALSE) {
    printf("!no device presentation support!\n");
    return -2;
  }

  /* Find colour format */
  uint32_t format_c;
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->pdev, gpu->surface, &format_c,
                                       NULL);
  VkSurfaceFormatKHR formats[format_c];
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->pdev, gpu->surface, &format_c,
                                       formats);

  VkBool32 supported = VK_FALSE;
  for (uint32_t i = 0; i < format_c; i++) {
    if (formats[i].format == cfg_format &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      supported = VK_TRUE;
  }
  if (supported == VK_FALSE) {
    printf("!colour format not supported!\n");
    return 1;
  }

  /* Presentation mode */
  uint32_t mode_c;
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->pdev, gpu->surface, &mode_c,
                                            NULL);
  VkPresentModeKHR modes[mode_c];
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->pdev, gpu->surface, &mode_c,
                                            modes);

  supported = VK_FALSE;
  for (uint32_t i = 0; i < mode_c; i++) {
    if (modes[i] == VK_PRESENT_MODE_FIFO_KHR) supported = VK_TRUE;
  }
  if (supported == VK_FALSE) {
    printf("!presentation mode not supported!\n");
    return 2;
  }


  gpuExtFunctionsInit(gpu->ldev);

  return 0;
}

int gpuSwapchainCreate(struct GpuContext* gpu, uint32_t width,
                       uint32_t height) {
  VkSurfaceCapabilitiesKHR capable;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->pdev, gpu->surface, &capable);

  if (capable.currentExtent.width != gpu->extent.width ||
      capable.currentExtent.height != gpu->extent.width) {
    gpu->extent.width = width > capable.maxImageExtent.width
                            ? capable.maxImageExtent.width
                            : width;

    gpu->extent.width = width < capable.minImageExtent.width
                            ? capable.minImageExtent.width
                            : width;

    gpu->extent.height = height > capable.maxImageExtent.height
                             ? capable.maxImageExtent.height
                             : height;

    gpu->extent.height = height < capable.minImageExtent.height
                             ? capable.minImageExtent.height
                             : height;
  }

  uint32_t max_swapchain_c = capable.minImageCount + 1;
  if (capable.maxImageCount > 0 && max_swapchain_c > capable.maxImageCount)
    max_swapchain_c = capable.maxImageCount;

  gpu->frame_c = FRAMES_IN_FLIGHT;

  VkSwapchainCreateInfoKHR swapchain_create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = gpu->surface,
      .minImageCount = max_swapchain_c,
      .imageFormat = cfg_format,
      .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
      .imageExtent = gpu->extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      /* TODO: multiple queue indices ( current [0][0] ) */
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .preTransform = capable.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  if (vkCreateSwapchainKHR(gpu->ldev, &swapchain_create_info, NULL,
                           &gpu->swapchain) != VK_SUCCESS) {
    printf("!failed to create swap chain!\n");
    return 1;
  }

  /* Create Image Views */
  vkGetSwapchainImagesKHR(gpu->ldev, gpu->swapchain, &gpu->swapchain_c, NULL);
  gpu->swapchain_images = calloc(gpu->swapchain_c, sizeof(VkImage));
  gpu->swapchain_views = calloc(gpu->swapchain_c, sizeof(VkImageView));

  vkGetSwapchainImagesKHR(gpu->ldev, gpu->swapchain, &gpu->swapchain_c,
                          gpu->swapchain_images);

  for (uint32_t i = 0; i < gpu->swapchain_c; i++) {
    gpuImageViewCreate(gpu->ldev, gpu->swapchain_images[i],
                       &gpu->swapchain_views[i], cfg_format,
                       VK_IMAGE_ASPECT_COLOR_BIT);
  }

  return 0;
}

int gpuSwapchainDestroy(struct GpuContext* gpu) {
  for (uint32_t i = 0; i < gpu->swapchain_c; i++) {
    vkDestroyImageView(gpu->ldev, gpu->swapchain_views[i], NULL);
  }
  vkDestroySwapchainKHR(gpu->ldev, gpu->swapchain, NULL);

  return 0;
}

int gpuSwapchainRecreate(struct GpuContext* gpu, uint32_t width,
                         uint32_t height) {
  vkDeviceWaitIdle(gpu->ldev);

  gpuSwapchainDestroy(gpu);

  gpuSwapchainCreate(gpu, width, height);

  vkDeviceWaitIdle(gpu->ldev);

  return 0;
}

int gpuAuxiliaryCreate(struct GpuContext* gpu){
  /* VMA Creation */
  VmaAllocatorCreateInfo allocator_info = {
      .physicalDevice = gpu->pdev,
      .device = gpu->ldev,
      .instance = gpu->instance,
  };
  vmaCreateAllocator(&allocator_info, &gpu->allocator);

  /* Command Pool Creation */
  VkCommandPoolCreateInfo command_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = gpuQueueIndex(gpu),
  };

  if (vkCreateCommandPool(gpu->ldev, &command_pool_info, NULL,
                          &gpu->cmd_pool) != VK_SUCCESS) {
    printf("!failed to create command pool!");
    return 1;
  }

  /* Descriptor Pool Creation */
  VkDescriptorPoolSize pool_sizes[2];
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[0].descriptorCount = MAX_TILESETS;

  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[1].descriptorCount = MAX_LAYERS;

  VkDescriptorPoolCreateInfo descriptor_pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = 2,
      .pPoolSizes = pool_sizes,
      .maxSets = MAX_TILESETS + MAX_LAYERS,
      .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
  };

  if (vkCreateDescriptorPool(gpu->ldev, &descriptor_pool_info, NULL,
                             &gpu->descriptor_pool) != VK_SUCCESS)
    return 1;

  /* Time Sync Bits Creation */
  VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  gpu->image_available = calloc(gpu->frame_c, sizeof(VkSemaphore));
  gpu->render_finished = calloc(gpu->frame_c, sizeof(VkSemaphore));
  gpu->fence = calloc(gpu->frame_c, sizeof(VkFence));

  for (unsigned int i = 0; i < gpu->frame_c; i++) {
    if (vkCreateSemaphore(gpu->ldev, &semaphore_info, NULL,
                          &gpu->image_available[i]) != VK_SUCCESS ||
        vkCreateSemaphore(gpu->ldev, &semaphore_info, NULL,
                          &gpu->render_finished[i]) != VK_SUCCESS ||
        vkCreateFence(gpu->ldev, &fence_info, NULL, &gpu->fence[i]) !=
            VK_SUCCESS) {
      printf("!failed to create sync objects!\n");
      return 1;
    }
  }

  /* Command Buffer Creation */
  gpu->cmd_buffer = calloc(gpu->swapchain_c, sizeof(VkCommandBuffer));
  for (unsigned int i = 0; i < gpu->swapchain_c; i++)
    gpu->cmd_buffer[i] = VK_NULL_HANDLE;

  VkCommandBufferAllocateInfo allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = gpu->cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = gpu->swapchain_c,
  };

  if (vkAllocateCommandBuffers(gpu->ldev, &allocate_info, gpu->cmd_buffer) !=
      VK_SUCCESS)
    return 1;

  return 0;
}

int gpuTextureDescriptorsCreate(struct GpuContext* gpu){
  /* Texture Descriptor Sets Init */
  VkDescriptorSetLayoutBinding sampler_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = MAX_TILESETS,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  VkDescriptorBindingFlags bindless_flags =
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
      VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

  VkDescriptorSetLayoutBindingFlagsCreateInfo info2 = {
      .sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      .bindingCount = 1,
      .pBindingFlags = &bindless_flags,
  };

  VkDescriptorSetLayoutCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &sampler_binding,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
      .pNext = &info2,
  };

  if (vkCreateDescriptorSetLayout(gpu->ldev, &info, NULL,
                                  &gpu->texture_descriptors_layout) !=
      VK_SUCCESS)
    return 1;

  uint32_t max_binding = MAX_TILESETS - 1;
  VkDescriptorSetVariableDescriptorCountAllocateInfo count_info = {
      .sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
      .descriptorSetCount = 1,
      .pDescriptorCounts = &max_binding,
  };

  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = gpu->descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &gpu->texture_descriptors_layout,
      .pNext = &count_info,
  };

  if (vkAllocateDescriptorSets(gpu->ldev, &alloc_info,
                               &gpu->texture_descriptors) != VK_SUCCESS)
    return 1;

  return 0;
}

int gpuTexturesDescriptorsUpdate(struct GpuContext* gpu,
                                 struct TermTileset* tilesets, uint32_t count) {
  VkDescriptorImageInfo infos[count];
  VkWriteDescriptorSet writes[count];

  for (uint32_t i = 0; i < count; i++) {
    struct GpuImage* texture = &tilesets[i].image;
    if (texture->handle == VK_NULL_HANDLE) printf("texture loading error\n");

    infos[i] = (VkDescriptorImageInfo){
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = texture->view,
        .sampler = texture->sampler,
    };

    writes[i] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = gpu->texture_descriptors,
        .dstBinding = 0,
        .dstArrayElement = i,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &infos[i],
    };
  }

  vkUpdateDescriptorSets(gpu->ldev, count, writes, 0, NULL);

  return 0;
}


int gpuContextDestroy(struct GpuContext gpu) {
  // destroy sync objects
  for (unsigned int i = 0; i < gpu.frame_c; i++) {
    vkDestroySemaphore(gpu.ldev, gpu.image_available[i], NULL);
    vkDestroySemaphore(gpu.ldev, gpu.render_finished[i], NULL);
    vkDestroyFence(gpu.ldev, gpu.fence[i], NULL);
  }

  vkDestroyCommandPool(gpu.ldev, gpu.cmd_pool, NULL);
  vkDestroyPipelineLayout(gpu.ldev, gpu.pipeline_layout, NULL);
  vkDestroyPipeline(gpu.ldev, gpu.pipeline, NULL);
  vkDestroyDescriptorSetLayout(gpu.ldev, gpu.texture_descriptors_layout, NULL);
  vkDestroyDescriptorPool(gpu.ldev, gpu.descriptor_pool, NULL);

  gpuSwapchainDestroy(&gpu);

  vmaDestroyAllocator(gpu.allocator);

  vkDestroyDevice(gpu.ldev, NULL);
  vkDestroySurfaceKHR(gpu.instance, gpu.surface, NULL);
  vkDestroyInstance(gpu.instance, NULL);

  return 0;
}



static int gpuSamplerCreate(VkDevice ldev, VkPhysicalDevice pdev,
                            VkSampler* sampler) {
  VkPhysicalDeviceProperties properties = {0};
  vkGetPhysicalDeviceProperties(pdev, &properties);

  VkSamplerCreateInfo sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .mipLodBias = 0.0f,
      .minLod = 0.0f,
      .maxLod = 0.0f,
  };

  if (vkCreateSampler(ldev, &sampler_info, NULL, sampler) != VK_SUCCESS) {
    printf("!failed to create buffer\n!");
    return 1;
  }

  return 0;
}

int gpuImageToGpu(struct GpuContext* gpu, unsigned char* pixels, int width,
                  int height, int channels, struct GpuImage* texture) {
  VmaAllocatorInfo allocator_info;
  vmaGetAllocatorInfo(gpu->allocator, &allocator_info);

  VkFormat format;
  switch (channels) {
    case 4:
      format = VK_FORMAT_R8G8B8A8_SRGB;
      break;
    case 3:
      format = VK_FORMAT_R8G8B8_SRGB;
      break;
    case 1:
      format = VK_FORMAT_R8_UNORM;
      break;
    default:
      return 2;
  }

  VkDeviceSize image_size = width * height * channels;
  struct GpuBuffer image_b;
  gpuBufferCreate(gpu, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, image_size,
                  &image_b);

  /* copy pixel data to buffer */
  vmaCopyMemoryToAllocation(gpu->allocator, pixels, image_b.allocation, 0,
                            image_size);

  gpuImageAlloc(
      gpu->allocator, texture,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, format,
      width, height);

  /* Copy the image buffer to a VkImage proper */
  VkCommandBuffer cmd = gpuCmdSingleBegin(gpu);
  transitionImageLayout(cmd, texture->handle,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  gpuCmdSingleEnd(gpu, cmd);

  gpuCopyBufferToImage(gpu, image_b.handle, texture->handle,
                                     (uint32_t)width, (uint32_t)height);

  gpuBufferDestroy(gpu->allocator, &image_b);

  cmd = gpuCmdSingleBegin(gpu);
  transitionImageLayout(
      cmd, texture->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  gpuCmdSingleEnd(gpu, cmd);

  /* Create image view */
  gpuImageViewCreate(allocator_info.device, texture->handle,
                                   &texture->view, format,
                                   VK_IMAGE_ASPECT_COLOR_BIT);
  gpuSamplerCreate( allocator_info.device, 
          allocator_info.physicalDevice, &texture->sampler);

  return 0;
}

