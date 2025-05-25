#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"

#define UV_BITS 10
#define ATLAS_BITS 6
#define FG_BITS 8
#define BG_BITS 8

#define BG_OFF 0
#define FG_OFF (BG_OFF + BG_BITS)
#define ATLAS_OFF (FG_OFF + FG_BITS)
#define UV_OFF (ATLAS_OFF + ATLAS_BITS)

#define MASK(bits) ((1u << (bits)) - 1)

static inline uint32_t glyphPackUnicode(struct TermContext* term,
                                        uint32_t unicode, uint32_t atlas,
                                        uint32_t fg, uint32_t bg) {
  if (atlas > MAX_TILESETS) return 1;
  if (term->tile_data[atlas].image.handle == NULL) {
    atlas = ASCII_TEXTURE_INDEX;
  }
  struct TermTileset tex = term->tile_data[atlas];
  int32_t uv = tex.decoder(tex.encodings, tex.glyph_c, unicode);

  return ((uint32_t)(uv & MASK(UV_BITS)) << UV_OFF) |
         ((atlas & MASK(ATLAS_BITS)) << ATLAS_OFF) |
         ((fg & MASK(FG_BITS)) << FG_OFF) | (bg & MASK(BG_BITS));
}

uint32_t pack16into32(uint16_t a, uint16_t b) {
  return (uint32_t)a << 16 | (uint32_t)b;
}

void termMove(struct TermContext* gfx, int32_t x, int32_t y) {
  gfx->cursor[0] = iMin(x, gfx->width_in_tiles);
  gfx->cursor[1] = iMin(y, gfx->height_in_tiles);
}

void termAddCh(struct TermContext* term, uint32_t unicode) {
  if (term->cursor[0] > term->width_in_tiles) {
    term->cursor[1] += 1;
    term->cursor[0] -= term->width_in_tiles;
  }

  struct GpuPackedTile dst = {
      .pos = pack16into32(term->cursor[0], term->cursor[1]),
      .unicode_atlas_and_colors =
          glyphPackUnicode(term, unicode, term->atlas, term->fg, term->bg)};

  size_t layer_offset = (term->width_in_tiles * term->height_in_tiles) * 0;
  size_t i = ( term->cursor[1] * term->width_in_tiles ) + term->cursor[0];

  struct GpuPackedTile* b = gpuBufferGetPtr(term->gpu.allocator, term->tile_indices);
  b[layer_offset + i] = dst;
  term->cursor[0]++;
}

void updateMvpUbo(struct TermContext* ctx){

//static float rot = 0;
 //      rot += 0.1;

 struct GpuMvp mvp;
 glm_mat4_identity(mvp.model);

 glm_mat4_identity(mvp.view);
 vec3 eye = {0.0f, 0.0f, 5.0f};    // Camera position
 vec3 center = {0.0f, 0.0f, 0.0f};    // Look at origin
 vec3 up = {0.0f, 1.0f, 0.0f};        // Up vector
 glm_lookat(eye, center, up, mvp.view);

 glm_mat4_identity(mvp.proj);
 glm_ortho(0, ASCII_SCREEN_WIDTH *2,
        0, ASCII_SCREEN_HEIGHT *2,
        -10.0f, 10.0f, mvp.proj);

//printf("writing %p\n", (void*)(&ctx->transform_ubo));
 memcpy(gpuBufferGetPtr(ctx->gpu.allocator, ctx->transform_ubo), &mvp, sizeof(struct GpuMvp));
}
void termMvAddCh(struct TermContext* gfx, int32_t x, int32_t y,
                 uint32_t unicode) {
  termMove(gfx, x, y);
  termAddCh(gfx, unicode);
}

void termPrint(struct TermContext* term, const char* str) {
  term->atlas = ASCII_TEXTURE_INDEX;

  int i = 0;
  char ch;
  while ((ch = str[i])) {
    if (ch == '\n') {
      term->cursor[1] += 1;
      term->cursor[0] = 0;
    } else {
      termAddCh(term, str[i]);
    }
    i++;
  }
  // end
}

int gfxBakeCommandBuffer(struct TermContext* term) {
  struct GpuContext* gpu = &term->gpu;
  VkCommandBuffer cmd_b = gpu->cmd_buffer[gpu->swapchain_x];
  vkResetCommandBuffer(cmd_b, 0);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = 0,
      .pInheritanceInfo = NULL,
  };

  if (vkBeginCommandBuffer(cmd_b, &begin_info) != VK_SUCCESS) {
    printf("!failed to begin recording command buffer!\n");
  }

  transitionImageLayout(cmd_b, gpu->swapchain_images[gpu->swapchain_x],
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  transitionImageLayout(cmd_b, gpu->swapchain_images[gpu->swapchain_x],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VkClearValue clear_value;
  clear_value.color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};

  VkRenderingAttachmentInfoKHR color_attachment_info = {
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView = gpu->swapchain_views[gpu->swapchain_x],
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clear_value,
  };

  VkRenderingInfoKHR render_info = {
      VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .renderArea.offset = {0, 0},
      .renderArea.extent = gpu->extent,
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
  };
  pfn_vkCmdBeginRenderingKHR(cmd_b, &render_info);

  GfxPushConstant constants = {
      .screen_size_px = {gpu->extent.width, gpu->extent.height}
  };
  vkCmdPushConstants(cmd_b, gpu->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(GfxPushConstant), &constants);

  vkCmdBindDescriptorSets(cmd_b, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            gpu->pipeline_layout, 0, 1, &gpu->transform_descriptors, 0, NULL);

  vkCmdBindDescriptorSets(cmd_b, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          gpu->pipeline_layout, 1, 1, &gpu->texture_descriptors,
                          0, NULL);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = gpu->extent.width * ASCII_SCALE,
      .height = gpu->extent.height * ASCII_SCALE,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(cmd_b, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = gpu->extent,
  };

  vkCmdSetScissor(cmd_b, 0, 1, &scissor);
  vkCmdBindPipeline(cmd_b, VK_PIPELINE_BIND_POINT_GRAPHICS, gpu->pipeline);

  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd_b, 0, 1, &term->tile_indices.handle, offsets);

  vkCmdDrawIndirect(cmd_b, term->indirect.handle, 0,
                    MAX_LAYERS,
                    sizeof(VkDrawIndirectCommand));

  pfn_vkCmdEndRenderingKHR(cmd_b);

  if (vkEndCommandBuffer(cmd_b) != VK_SUCCESS) {
    printf("!failed to record command buffer!");
    return 1;
  }
  return 0;
}

void termDrawRefresh(struct TermContext* term) {
  struct GpuContext* gpu = &term->gpu;
 
  updateMvpUbo(term);
  gpu->frame_x = (gpu->frame_x + 1) % gpu->frame_c;
 
  // waits for gpu to finish rendering
  VkResult fence_result = 
      vkWaitForFences(gpu->ldev, 1, &gpu->fence[gpu->frame_x],
                                 VK_TRUE, UINT32_MAX);
  
  if (fence_result == VK_TIMEOUT) {
    printf("FATAL: VkWaitForFences timed out\n");
    abort();
  }

  VkSemaphore* image_available = &gpu->image_available[gpu->frame_x];
  VkResult result = VK_TIMEOUT;
 
  result = vkAcquireNextImageKHR(gpu->ldev, gpu->swapchain, UINT32_MAX,
                                 *image_available, VK_NULL_HANDLE,
                                 &gpu->swapchain_x);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    int width_px, height_px;
    glfwGetWindowSize(term->window, &width_px, &height_px);

    gpuSwapchainRecreate(&term->gpu, width_px, height_px);
    vkDestroySemaphore(gpu->ldev, *image_available, NULL);
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    
    vkCreateSemaphore(gpu->ldev, &semaphore_info, NULL, image_available);
    return;
  }

  vkResetFences(gpu->ldev, 1, &gpu->fence[gpu->frame_x]);
  gfxBakeCommandBuffer(term);

  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = image_available,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &gpu->cmd_buffer[gpu->swapchain_x],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &gpu->render_finished[gpu->frame_x],
  };

  // waits on VkAcquireImageKHR to signal an image is available
  vkQueueSubmit(gpu->queue, 1, &submit_info, gpu->fence[gpu->frame_x]);

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &gpu->render_finished[gpu->frame_x],
      .swapchainCount = 1,
      .pSwapchains = &gpu->swapchain,
      .pImageIndices = &gpu->swapchain_x,
      .pResults = NULL};

  // waits on vkQueueSubmit to signal its finished rendering
  VkResult present_result = vkQueuePresentKHR(gpu->queue, &present_info);
  switch (present_result) {
    case VK_SUCCESS:
      break;
    case VK_ERROR_OUT_OF_DATE_KHR:
      break;
    case VK_SUBOPTIMAL_KHR:
      break;
    default:
      return;
  }

  termPollInput(term);  
}

