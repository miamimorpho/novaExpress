#ifndef VKTERM_PUBLIC_H
#define VKTERM_PUBLIC_H
#include <stdint.h>

#include "maths.h"
#include "vulkan_backend.h"
#include "allocator.h"
#include "../extern/cglm/include/cglm/cglm.h"

/* Renderer Settings */
#define TILE_BUFFER_WIDTH 32
#define TILE_BUFFER_SIZE ( TILE_BUFFER_WIDTH * TILE_BUFFER_WIDTH )

/* Unicode Magic Numbers */
#define PUA_START (0xE000)  // 57343
/* provides 6400 chars */
#define PUA_LEFT_CLICK (PUA_START + GLFW_MOUSE_BUTTON_LEFT)

typedef struct TermContext* TermCtx;

struct TermInputComm {
  uint32_t unicode;
  double mouse_x;
  double mouse_y;
};

struct GpuPushConstant{
    int cam_mode;
    int sprite_mode;
};

struct GpuFrameUniform {
   mat4 cam_flat_view;
   mat4 cam_flat_proj;
   mat4 cam_seven_view;
   mat4 cam_seven_proj;
};

struct TermContext {
  GLFWwindow* window;
  struct GpuContext* gpu; 
  struct TermTileset tile_data[MAX_TILESETS];

  struct GpuBuffer* frame_ubo;              // per frame data
  struct GpuPushConstant* draw_push_const;  // per draw data
  struct GpuBuffer tile_indices;            // per quad data

  size_t sprite_count;
  struct GpuBuffer sprite_pos_arr;
  struct GpuBuffer sprite_indices;

  Allocator allocator;

  ivec2 cursor;
  int32_t layer;
  int32_t atlas;
  int32_t fg;
  int32_t bg;
};

/* Init + Settings */
struct TermContext* termCtxCreate(int, int);
int termCtxDestroy(struct TermContext*);
int gpuTilesetLoad(struct TermContext*, const char*);

/* drawing.c */
void termDrawBegin(struct TermContext*);

void tileMoveSafe(struct TermContext*, int32_t x, int32_t y);
void termAtlas(struct TermContext*, int32_t);
void termFg(struct TermContext*, int32_t);
void termBg(struct TermContext*, int32_t);
void tileAdd(struct TermContext*, uint32_t);
void tileMvAdd(struct TermContext*, int32_t, int32_t, uint32_t);
void tilePrint(struct TermContext*, const char*);

int spriteAdd(struct TermContext*, const vec2*, const uint32_t*, size_t);
int spriteMove(struct TermContext* term, vec2, size_t, size_t);

void termDrawRefresh(struct TermContext*);
void termDrawEnd(struct TermContext*);

void termPollInput(struct TermContext*);
struct TermInputComm termGetInput(struct TermContext*);
#define IS_MOUSE_PRESSED(unicode) \
  (((unicode) >= PUA_START) ? (unicode) - PUA_START + 1 : 0)

int termExitState(void);

#endif  // VKTERM_PUBLIC_H
