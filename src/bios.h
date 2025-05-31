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

typedef struct {
  vec2 screen_size_px;
} GfxPushConstant;

struct GpuMvp {
   mat4 model;
   mat4 view;
   mat4 proj;
};

struct TermContext {
  GLFWwindow* window;
  struct GpuContext gpu; 

  struct TermTileset tile_data[MAX_TILESETS];
  struct GpuBuffer tile_indices;
  struct GpuBuffer indirect;
  // todo the indirect buffer should also behave as a 
  // layer control register, allowing the screen to be offset
  // by pixel accuracy and maybe later tilt the entire render
  // mode 7

  struct GpuBuffer transform_ubo;

  Allocator allocator;

  // ui layout state
  // make argument
  ivec2 cursor;
  int32_t layer;
  int32_t atlas;
  int32_t fg;
  int32_t bg;
};

/* Init + Settings */
struct TermContext* termCtxCreate(int, int);
int termCtxDestroy(struct TermContext*);
int gfxTilesetLoad(struct TermContext*, const char*);

/* drawing.c */
void termDrawBegin(struct TermContext*);
void termMove(struct TermContext*, int32_t x, int32_t y);
void termAtlas(struct TermContext*, int32_t);
void termFg(struct TermContext*, int32_t);
void termBg(struct TermContext*, int32_t);
void termAddCh(struct TermContext*, uint32_t);
void termMvAddCh(struct TermContext*, int32_t, int32_t, uint32_t);
void termPrint(struct TermContext*, const char*);
void termDrawRefresh(struct TermContext*);
void termDrawEnd(struct TermContext*);

void termPollInput(struct TermContext*);
struct TermInputComm termGetInput(struct TermContext*);
#define IS_MOUSE_PRESSED(unicode) \
  (((unicode) >= PUA_START) ? (unicode) - PUA_START + 1 : 0)

int termExitState(void);

#endif  // VKTERM_PUBLIC_H
