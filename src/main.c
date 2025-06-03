#include "main.h"

int main(void) {
  TermCtx term = termCtxCreate(
          TILE_BUFFER_WIDTH,
          TILE_BUFFER_WIDTH);

  gpuTilesetLoad(term, "textures/color.png");
  gpuTilesetLoad(term, "textures/icl8x8u.bdf");
  gpuTilesetLoad(term, "textures/mrmotext-ex11.png");
  
  Allocator arena = arenaCreate(10 * MB, NULL);
  struct Map map = mapCreate(arena);
  dungeonBuild(&map);

  struct Mobile *player = mobCreate(map, 3, 3);
  mobName(player, "player");
  player->tile.unicode = 417;
  player->tile.atlas = 2;

  vec2 pos_arr[] = {{0.0f, 0.0f}};
  uint32_t sprt_tiles[] = {4};
  spriteAdd(term, pos_arr, sprt_tiles, 1);

  struct Mobile *goblin = mobCreate(map, 6, 7);
  mobName(goblin, "goblin");
  goblin->tile.unicode = 907;
  goblin->tile.atlas = 2;

  portalCreate(map, 0, 0, 16, 16);
  portalCreate(map, 16, 16, 0, 0);

  Allocator fov_allocator = arenaCreate(FOV_ALLOCATOR_RAM_SIZE, NULL);

  term->layer = 1;
  tilePrint(term, "test");
  term->layer = 0;

  while (1) {
    fovDrawWorld(term, &map, player->hash_pos_entry.x, player->hash_pos_entry.y, fov_allocator);
    turnUser(term, &map, player);
    //spriteMove(term, (vec2){0.1f, 0.1f}, 0, 1);
    termDrawRefresh(term);
  }

  arenaDestroy(fov_allocator);
  arenaDestroy(arena);
  termCtxDestroy(term);
  return 0;
}
