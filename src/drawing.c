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

// TODO: standardise bit packing through code
uint32_t gpuPackUnicode(struct TermContext* term,
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

void tileMoveSafe(struct TermContext* gfx, int32_t x, int32_t y) {
  gfx->cursor[0] = iMin(x, TILE_BUFFER_WIDTH);
  gfx->cursor[1] = iMin(y, TILE_BUFFER_WIDTH);
}

void tileAdd(struct TermContext* term, uint32_t unicode) {
  if (term->cursor[0] > TILE_BUFFER_WIDTH) {
    term->cursor[1] += 1;
    term->cursor[0] -= TILE_BUFFER_WIDTH;
  }

  size_t layer_offset = TILE_BUFFER_SIZE * term->layer;
  size_t i = ( term->cursor[1] * TILE_BUFFER_WIDTH ) + term->cursor[0];

  uint32_t* b = gpuBufferGetPtr(term->gpu, term->tile_indices);
  b[layer_offset + i] = gpuPackUnicode(term, unicode, term->atlas, term->fg, term->bg);
  term->cursor[0]++;
}

void tileMvAdd(struct TermContext* gfx, int32_t x, int32_t y,
                 uint32_t unicode) {
  tileMoveSafe(gfx, x, y);
  tileAdd(gfx, unicode);
}

void tilePrint(struct TermContext* term, const char* str) {
  term->atlas = ASCII_TEXTURE_INDEX;

  int i = 0;
  char ch;
  while ((ch = str[i])) {
    if (ch == '\n') {
      term->cursor[1] += 1;
      term->cursor[0] = 0;
    } else {
      tileAdd(term, str[i]);
    }
    i++;
  }
  // end
}

int spriteAdd(struct TermContext* term, const vec2* pos_arr, const uint32_t* tiles, size_t count) {

  if(term->sprite_count + count > MAX_SPRITES) abort();

  gpuBufferPush(term->gpu, &term->sprite_pos_arr, pos_arr, count * sizeof(vec2));
  gpuBufferPush(term->gpu, &term->sprite_indices, tiles, count * sizeof(uint32_t));
  term->sprite_count += count;

  return 0;
}

int spriteMove(struct TermContext* term, vec2 delta, size_t start, size_t count){

  if(start + count > MAX_SPRITES) abort();
  
  vec2* pos_arr_ptr = gpuBufferGetPtr(term->gpu, term->sprite_pos_arr);
  for(size_t i = start; i < start + count; i++){
   glm_vec2_add(delta, pos_arr_ptr[i], pos_arr_ptr[i]); 
  }
  return 0;
}

int gfxBakeCommandBuffer(struct TermContext* term) {
  struct GpuContext* gpu = term->gpu;
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

  vkCmdBindDescriptorSets(cmd_b, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            gpu->pipeline_layout, 0, 1, &gpu->frame_descriptors, 0, NULL);

  vkCmdBindDescriptorSets(cmd_b, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          gpu->pipeline_layout, 1, 1, &gpu->texture_descriptors,
                          0, NULL);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = gpu->extent.width, //* ASCII_SCALE,
      .height = gpu->extent.height, //* ASCII_SCALE,
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

  // Placeholder sprite vertex buffer for now
  vkCmdBindVertexBuffers(cmd_b, 0, 1, &term->sprite_pos_arr.handle, offsets);

  // Tile map rendering
  vkCmdBindVertexBuffers(cmd_b, 1, 1, &term->tile_indices.handle, offsets);
  for(int i = 0; i < MAX_LAYERS; i++){
    vkCmdPushConstants(cmd_b,
            gpu->pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(struct GpuPushConstant),
            &term->draw_push_const[i]);

      vkCmdDraw(cmd_b,
              6,
              TILE_BUFFER_SIZE,
              0,
              i * TILE_BUFFER_SIZE);
  }

  // Sprite rendering
  vkCmdBindVertexBuffers(cmd_b, 1, 1, &term->sprite_indices.handle, offsets);
  
  struct GpuPushConstant sprite_push = (struct GpuPushConstant){3, 1};
  vkCmdPushConstants(cmd_b,
          gpu->pipeline_layout,
          VK_SHADER_STAGE_VERTEX_BIT,
          0,
          sizeof(struct GpuPushConstant),
          &sprite_push);
  vkCmdDraw(cmd_b, 6, term->sprite_count, 0, 0);

  pfn_vkCmdEndRenderingKHR(cmd_b);

  if (vkEndCommandBuffer(cmd_b) != VK_SUCCESS) {
    printf("!failed to record command buffer!");
    return 1;
  }
  return 0;
}

void termDrawRefresh(struct TermContext* term) {
  struct GpuContext* gpu = term->gpu;
 
  //updateMvpUbo(term);
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

    gpuSwapchainRecreate(term->gpu, width_px, height_px);
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
