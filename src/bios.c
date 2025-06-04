#include <stdio.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"

#include "bios.h"
#include "vulkan_backend.h"

typedef struct {
  double time;
  uint32_t unicode;
  double mouse_x_norm;
  double mouse_y_norm;
} TermInputState;

static TermInputState input_;

/* TEXTURES */
uint32_t DecoderArrayGet(uint32_t* encodings, uint32_t count,
                         uint32_t unicode) {
  if (unicode < count) {
    return encodings[unicode];
  }
  return 0;
}

int bsearchCompare(const void* a, const void* b) {
  return (*(uint32_t*)a - *(uint32_t*)b);
}

uint32_t DecoderBinarySearch(uint32_t* encodings, uint32_t count,
                             uint32_t unicode) {
  uint32_t* result = 
      (uint32_t*)bsearch(&unicode, encodings, count,
              sizeof(uint32_t), bsearchCompare);
  if (result) return result - encodings;

  return 0;
}

uint32_t DecoderLinearSearch(uint32_t* encodings, uint32_t count,
                             uint32_t unicode) {
  if (unicode > count) return 0;
  for (uint32_t i = 0; i < count; i++) {
    if (encodings[i] == unicode) {
      return i;
    }
  }
  return 0;
}

uint8_t* pngFileLoad(const char* filename, struct TermTileset* tileset) {
  int x, y, n;
  stbi_info(filename, &x, &y, &n);
  uint8_t* pixels = stbi_load(filename, &x, &y, &n, n);
  if (pixels == NULL) {
    printf("%s\n", stbi_failure_reason());
    return NULL;
  }
  // size_t size = x * y * n;
  tileset->image_w = (uint32_t)x;
  tileset->image_h = (uint32_t)y;
  tileset->glyph_w = ASCII_TILE_SIZE;
  tileset->glyph_h = ASCII_TILE_SIZE;
  tileset->glyph_c = (x * y) / (ASCII_TILE_SIZE * ASCII_TILE_SIZE);
  tileset->channels = n;

  tileset->encodings = calloc(tileset->glyph_c, sizeof(uint32_t));
  for (uint32_t i = 0; i < tileset->glyph_c; i++) {
    tileset->encodings[i] = i;
  }
  tileset->decoder = &DecoderArrayGet;

  return pixels;
}

int HexToUINT4(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  return -1;
}

uint8_t* bdfFileLoad(const char* filename, struct TermTileset* font) {
  const int L_LEN = 80;
  const int W_LEN = 8;
  char x_s[W_LEN];
  char y_s[W_LEN];
  char x_offset_s[W_LEN];
  char y_offset_s[W_LEN];

  int font_y_offset;

  FILE* fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Error opening font file\n");
    return NULL;
  }

  // bdf files are parsed line by plaintext line
  char line[L_LEN];
  char prefix[L_LEN];

  int supported_c = 0;
  /* METADATA LOADING START */
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (supported_c == 2) break;

    sscanf(line, " %s", prefix);

    if (strcmp(prefix, "FONTBOUNDINGBOX") == 0) {
      sscanf(line, "%*s %s %s %s %s", x_s, y_s, x_offset_s, y_offset_s);

      font->glyph_w = atoi(x_s);
      font->glyph_h = atoi(y_s);
      font_y_offset = atoi(y_offset_s);

      supported_c += 1;
      continue;
    }

    if (strcmp(prefix, "CHARS") == 0) {
      char glyph_c_s[W_LEN];
      sscanf(line, "%*s %s", glyph_c_s);
      font->glyph_c = atoi(glyph_c_s);
      supported_c += 1;
      continue;
    }
  }
  if (supported_c != 2) {
    fprintf(stderr, "bdfFileLoad() not enough config information\n");
    return NULL;
  } /* META LOADING END */

  font->encodings = calloc(font->glyph_c, sizeof(uint32_t));

  // allocate pixel data to Z order array
  font->channels = 1;
  font->image_w = font->glyph_w * ATLAS_WIDTH;
  font->image_h = font->glyph_h * ATLAS_WIDTH;
  size_t size = font->image_w * font->image_h;
  uint8_t* pixels = malloc(size);
  if (pixels == NULL) abort();
  memset(pixels, 0, size);

  rewind(fp);
  int glyph_i = 0;  // assuming C99 uses ASCII 437
  int glyph_y = -1;

  int bbx_size = 0;
  int bby_size = 0;
  int bbx_offset = 0;
  int bby_offset = 0;

  /* BYTE LOADING START */
  while (fgets(line, sizeof(line), fp) != NULL) {
    sscanf(line, "%s", prefix);

    if (strcmp(prefix, "ENDCHAR") == 0) {
      glyph_i++;
      glyph_y = -1;
      continue;
    }

    /* bdf files store 4 1-bit pixels across
     * as one hex char */
    if (glyph_y >= 0) {
      // int width_in_tiles = font->glyph_w * ATLAS_WIDTH;
      int atlas_y = glyph_i / ATLAS_WIDTH;
      int atlas_x = glyph_i % ATLAS_WIDTH;

      int dst_y = (atlas_y * font->glyph_h) + font_y_offset;  // atlas pixel pos
      dst_y += (8 - bby_size - bby_offset);                   // local pixel pos
      int dst_x = (atlas_x * font->glyph_w);
      dst_x += (8 - bbx_size) - bbx_offset;

      for (unsigned int i = 0; i < font->glyph_w; i++) {
        // read in one binary pixel
        uint8_t four_pixels = HexToUINT4(line[i / 4]);
        int pixel_index = 3 - (i % 4);
        uint8_t dst_pixel = ((four_pixels >> pixel_index) & 0x1);

        // draw in one uint8 pixel
        int dst_xy = ((dst_y + glyph_y) * font->image_w) + dst_x + i;
        pixels[dst_xy] = dst_pixel * 255;
      }
      glyph_y++;
      continue;
    }

    if (strcmp(prefix, "STARTCHAR") == 0) {
      char* glyph_name = strchr(line, ' ');
      if (glyph_name != NULL) glyph_name += 1;
      glyph_name[strlen(glyph_name) - 1] = '\0';
      continue;
    }

    if (strcmp(prefix, "ENCODING") == 0) {
      char code[W_LEN];
      sscanf(line, "%*s %s", code);
      font->encodings[glyph_i] = atoi(code);
      continue;
    }

    if (strcmp(prefix, "BBX") == 0) {
      sscanf(line, "%*s %s %s %s %s", x_s, y_s, x_offset_s, y_offset_s);
      bbx_size = atoi(x_s);
      bby_size = atoi(y_s);
      bbx_offset = atoi(x_offset_s);
      bby_offset = atoi(y_offset_s);
      continue;
    }

    if (strcmp(prefix, "BITMAP") == 0) {
      glyph_y = 0;
      continue;
    }
  } /* BYTE LOADING END*/
  font->decoder = &DecoderBinarySearch;

  fclose(fp);
  return pixels;
}

int gpuTilesetLoad(struct TermContext* term, const char* filename) {
  struct TermTileset* sets = term->tile_data;

  int atlas = -1;
  while (sets[++atlas].image.handle != NULL);
  if (atlas == -1 || atlas > MAX_TILESETS) return 1;
  struct TermTileset* dst = &sets[atlas];

  uint8_t* pixels = NULL;
  char* ext = strrchr(filename, '.');
  if (!ext || ext == filename) return 1;
  ext++;

  if (strcmp(ext, "png") == 0) pixels = pngFileLoad(filename, dst);

  if (strcmp(ext, "bdf") == 0) pixels = bdfFileLoad(filename, dst);

  if (pixels == NULL) {
    printf("cant read texture...\n");
    abort();
  }

  /* send image to GPU */
  if (gpuImageToGpu(term->gpu, pixels, dst->image_w, dst->image_h,
                    dst->channels, &dst->image) < 0)
    return 1;
  gpuTexturesDescriptorsUpdate(term->gpu, sets, atlas + 1);
  /* stbi_free() is just a wrapper for free() */
  free(pixels);

  return 0;
}

int gpuTilesetsFree(struct TermContext* term) {
  for (int i = 0; i < MAX_TILESETS; i++) {
    if (term->tile_data[i].image.handle != VK_NULL_HANDLE) {
      gpuImageDestroy(term->gpu, term->tile_data[i].image);
    }
  }
  return 0;
}

void cameraView(int rot, int elev, mat4 dest){
    float dist = 70;
    vec3 center = {
	TILE_BUFFER_WIDTH / 2,
	TILE_BUFFER_WIDTH / 2,
	0
    };
    vec3 eye = {
        center[0] + dist * cos(glm_rad(rot)) * cos(glm_rad(elev)),
        center[1] + dist * sin(glm_rad(rot)) * cos(glm_rad(elev)), 
        center[2] + dist * sin(glm_rad(elev))
    };
    vec3 up = {0};
    if(elev >= 90 ){
        up[1] = 1;
        up[2] = 0;
    }else{
        up[1] = 0;
        up[2] = -1;
    }

    glm_lookat(eye, center, up, dest);
}

/* Isometric, Rot -135, Elev 30
 * Top Dow, Rot -90, Elev 90
 *
 *
 */
void cameraOrtho(mat4 out){
    int scr_width = TILE_BUFFER_WIDTH * 0.5; 
    int scr_height = TILE_BUFFER_WIDTH * 0.5;

    glm_ortho(-scr_width, scr_width, 
              -scr_height, scr_height, 
              0.01f, 100.0f, out);
}

void cameraPersp(mat4 out){
    glm_perspective(glm_rad(30), 1.0f, 0.0f, 100.0f, out);
}

// used for debugging only
void camera2D(mat4 dest){
    mat4 model, view, proj;
    glm_mat4_identity(model);
    glm_mat4_identity(view);
    
    glm_ortho(0, TILE_BUFFER_WIDTH,
	      0, TILE_BUFFER_WIDTH,
	      -1.0f, 1.0f, proj);
    
    glm_mat4_mulN((mat4 *[]){&proj, &view, &model}, 3, dest);
}

void termPollInput(struct TermContext* term) {

  const double press_delay = 1;//(float)1 / (float)30;

  input_.unicode = 0;
  glfwWaitEventsTimeout(press_delay);

  double xpos, ypos;
  glfwGetCursorPos(term->window, &xpos, &ypos);

  // TODO: add mouse input back
  //input_.mouse_x_norm = xpos / term->gpu.extent.width;
  //input_.mouse_y_norm = ypos / term->gpu.extent.height;
}

struct TermInputComm termGetInput(struct TermContext* term) {
  return (struct TermInputComm){
      input_.unicode,
      input_.mouse_x_norm,
      input_.mouse_y_norm,
  };
}

void characterCallback(GLFWwindow* window, unsigned int codepoint) {
  input_.time = glfwGetTime();
  input_.unicode = codepoint;
}

void closeCallback(GLFWwindow* window) { exit(0); }

void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
  if (action == GLFW_PRESS) {
    input_.time = glfwGetTime();
    input_.unicode = PUA_START + button;
  }
}

void termInputContextCreate(GLFWwindow* window) {
  glfwMakeContextCurrent(window);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetCursorPos(window, 0, 0);

  glfwSetWindowCloseCallback(window, closeCallback);
  glfwSetCharCallback(window, characterCallback);
  // glfwSetKeyCallback(window, keyCallback);
  glfwSetMouseButtonCallback(window, mouseCallback);

  input_.time = 0;
}

/* need a seperate buffer per frame in flight */
int frameDescriptorsCreate(struct GpuContext* gpu, struct GpuBuffer* buffers, size_t count){

  /* Transformation Buffer */
  VkDescriptorSetLayoutBinding transform_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &transform_binding,
  };

  if(vkCreateDescriptorSetLayout(gpu->ldev, &layout_info, NULL, &gpu->frame_descriptors_layout)) abort();

  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = gpu->descriptor_pool,
      .descriptorSetCount = FRAMES_IN_FLIGHT,
      .pSetLayouts = &gpu->frame_descriptors_layout
  };

  VK_CHECK(vkAllocateDescriptorSets(gpu->ldev, &alloc_info, &gpu->frame_descriptors));

  for(size_t i = 0; i < count; i++){
    gpuBufferCreate(gpu, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            sizeof(struct GpuFrameUniform), &buffers[i]);
    printf("created %p\n", (void*)buffers);
    VkDescriptorBufferInfo buffer_info = {
        .buffer = buffers[i].handle,
        .offset = 0,
        .range = sizeof(struct GpuFrameUniform),
    };

    VkWriteDescriptorSet descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = gpu->frame_descriptors,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .pBufferInfo = &buffer_info,
    };
    vkUpdateDescriptorSets(gpu->ldev, 1, &descriptor_write, 0, NULL);
  }
  return 0;
}

int gpuPipelineCreate(struct GpuContext* gpu) {
  VkPushConstantRange push_constant = {
      .offset = 0,
      .size = sizeof(struct GpuPushConstant),
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };

  VkDescriptorSetLayout descriptor_layouts[2] = {
     gpu->frame_descriptors_layout,  gpu->texture_descriptors_layout
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 2,
      .pSetLayouts = descriptor_layouts,
      .pPushConstantRanges = &push_constant,
      .pushConstantRangeCount = 1,
  };

  if (vkCreatePipelineLayout(gpu->ldev, &pipeline_layout_info, NULL,
                             &gpu->pipeline_layout) != VK_SUCCESS) {
    printf("!failed to create pipeline layout!\n");
    return 1;
  }

  VkShaderModule vert_shader;
  gpuSpvLoad(gpu->ldev, "shaders/vert.spv", &vert_shader);
  VkShaderModule frag_shader;
  gpuSpvLoad(gpu->ldev, "shaders/frag.spv", &frag_shader);

  VkPipelineShaderStageCreateInfo vert_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vert_shader,
      .pName = "main",
      .pSpecializationInfo = NULL,
  };

  VkPipelineShaderStageCreateInfo frag_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = frag_shader,
      .pName = "main",
      .pSpecializationInfo = NULL,
  };

  VkPipelineShaderStageCreateInfo shader_stages[2] = {vert_info, frag_info};

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
      .stencilTestEnable = VK_FALSE,
      .front = {0},
      .back = {0},
  };  // Model Specific

  VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
      .pDynamicStates = dynamic_states};


  // Vertex Buffer Creation
  VkVertexInputBindingDescription vtx_bindings[] = {
      {
        .binding = 0,
        .stride = sizeof(vec2),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
      },
      {
          .binding = 1,
          .stride = sizeof(uint32_t),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
      }};

  VkVertexInputAttributeDescription vtx_attributes[] = {
    {
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = 0,
    },
    {
        .binding = 1,
        .location = 1,
        .format = VK_FORMAT_R32_UINT,
        .offset = 0,
    }};

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 2,
      .vertexAttributeDescriptionCount = 2,
      .pVertexBindingDescriptions = vtx_bindings,
      .pVertexAttributeDescriptions = vtx_attributes,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  // negative viewport size creates opengl style coordinates
  // features of maintenece extension 1
  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = gpu->extent.width,
      .height = -gpu->extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = gpu->extent,
  };

  VkPipelineViewportStateCreateInfo viewport_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  VkPipelineRasterizationStateCreateInfo rasterization_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisample_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .minSampleShading = 1.0f,
      .pSampleMask = NULL,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,                             // Optional
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // Optional
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // Optional
      .alphaBlendOp = VK_BLEND_OP_ADD,                             // Optional
  };

  VkPipelineColorBlendStateCreateInfo color_blend_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants[0] = 0.0f,
      .blendConstants[1] = 0.0f,
      .blendConstants[2] = 0.0f,
      .blendConstants[3] = 0.0f,
  };

  VkFormat cfg_format_val = cfg_format;
  VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &cfg_format_val,
  };

  VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly_info,
      .pViewportState = &viewport_info,
      .pRasterizationState = &rasterization_info,
      .pMultisampleState = &multisample_info,
      .pDepthStencilState = &depth_stencil,
      .pColorBlendState = &color_blend_info,
      .pDynamicState = &dynamic_state_info,
      .layout = gpu->pipeline_layout,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
      .pNext = &pipeline_rendering_create_info,
  };

  if (vkCreateGraphicsPipelines(gpu->ldev, VK_NULL_HANDLE, 1, &pipeline_info,
                                NULL, &gpu->pipeline) != VK_SUCCESS) {
    printf("!failed to create graphics pipeline!\n");
  }

  vkDestroyShaderModule(gpu->ldev, frag_shader, NULL);
  vkDestroyShaderModule(gpu->ldev, vert_shader, NULL);

  return 0;
}

struct TermContext* termCtxCreate(int width_in_tiles, int height_in_tiles) {
  struct TermContext* term = malloc(sizeof(struct TermContext));
  *term = (struct TermContext){0}; // initialise to 0

  if (!glfwInit()) {
    printf("glfw init failure\n");
    abort();
  }

  //glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  int width = width_in_tiles * ASCII_TILE_SIZE;
  int height = height_in_tiles * ASCII_TILE_SIZE;
  if(!(term->window = glfwCreateWindow(width, height, "Vulkan/GLFW", NULL, NULL))){
    printf("glfw init failed\n");
    abort();
  }

  term->gpu = malloc(sizeof(struct GpuContext));
  gpuDevicesCreate(term->gpu, term->window);

  int width_px, height_px;
  glfwGetWindowSize(term->window, &width_px, &height_px);
  gpuSwapchainCreate(term->gpu, width_px, height_px);
  term->gpu->swapchain_x = 0;
  term->gpu->frame_x = 0;

  gpuAuxiliaryCreate(term->gpu);

  gpuTextureDescriptorsCreate(term->gpu);

  term->frame_ubo = calloc( FRAMES_IN_FLIGHT, sizeof(struct GpuBuffer));
  frameDescriptorsCreate(term->gpu, term->frame_ubo, FRAMES_IN_FLIGHT);
  gpuPipelineCreate(term->gpu);

  termInputContextCreate(term->window);
  
  term->allocator = arenaCreate( 5 * MB, NULL);
  
  gpuBufferCreate(term->gpu,
		  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		  MAX_LAYERS * TILE_BUFFER_SIZE * sizeof(uint32_t),
		  &term->tile_indices);

  term->draw_push_const = 
      fatPtrCreate(MAX_LAYERS, sizeof(struct GpuPushConstant), term->allocator);

  term->draw_push_const[0].cam_mode = 2;
  term->draw_push_const[1].cam_mode = 1;

  struct GpuFrameUniform frame_ubo;
  cameraView(-90, 90, frame_ubo.cam_flat_view);
  cameraOrtho(frame_ubo.cam_flat_proj);
  
  cameraView(45, 45, frame_ubo.cam_seven_view);
  cameraPersp(frame_ubo.cam_seven_proj);

  gpuBufferPush(term->gpu, &term->frame_ubo[0], &frame_ubo, sizeof(frame_ubo));

  gpuBufferCreate(term->gpu,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          MAX_SPRITES * sizeof(uint32_t),
          &term->sprite_indices);

  gpuBufferCreate(term->gpu,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          MAX_SPRITES * sizeof(vec2),
          &term->sprite_pos_arr);

  glm_ivec2_zero(term->cursor);

  term->layer = 0;
  term->atlas = 0;
  term->fg = 15;
  term->bg = 0;

  return term;
}

int termCtxDestroy(struct TermContext* term) {
  struct GpuContext* gpu = term->gpu;

  // End last frame
  VkResult result;
  for (unsigned int i = 0; i < gpu->frame_c; i++) {
    result = vkWaitForFences(gpu->ldev, 1, &gpu->fence[i], VK_TRUE, UINT32_MAX);
    if (result == VK_TIMEOUT) {
      printf("FATAL: VkWaitForFences timed out\n");
      // protection from deadlocks
      abort();
    }
    vkResetCommandBuffer(gpu->cmd_buffer[i], 0);
  }

  gpuBufferDestroy(gpu->allocator, &term->tile_indices);

  gpuTilesetsFree(term);

  gpuContextDestroy(term->gpu);

  glfwDestroyWindow(term->window);
  glfwTerminate();

  arenaDestroy(term->allocator);

  return 0;
}
