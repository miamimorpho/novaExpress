#include "allocator.h"
#include "map.h"
#include "mobs.h"
#include "bios.h"

#define FOV_ALLOCATOR_RAM_SIZE (1 * KB)

typedef struct ShadowcastVTable_* ShadowcastVTable;
struct ShadowcastVTable_ {
  void (*renderTile)(ShadowcastVTable, uint32_t x, uint32_t y,
                     struct UnicodeTile);
};

int fovDrawWorld(TermCtx, struct Map*, uint32_t, uint32_t, Allocator);
