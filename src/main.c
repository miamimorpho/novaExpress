#include "main.h"

int main(void) {
  TermCtx term = termCtxCreate(
          ASCII_SCREEN_WIDTH,
          ASCII_SCREEN_HEIGHT);

  gfxTilesetLoad(term, "textures/color.png");
  gfxTilesetLoad(term, "textures/icl8x8u.bdf");
  gfxTilesetLoad(term, "textures/mrmotext-ex11.png");
  
  Allocator arena = arenaCreate(10 * MB, NULL);
  struct Map map = mapCreate(arena);
  dungeonBuild(&map);

  struct Mobile *player = mobCreate(map, 3, 3);
  mobName(player, "player");
  player->tile.unicode = 417;
  player->tile.atlas = 2;

  struct Mobile *goblin = mobCreate(map, 6, 7);
  mobName(goblin, "goblin");
  goblin->tile.unicode = 907;
  goblin->tile.atlas = 2;

  portalCreate(map, 0, 0, 16, 16);
  portalCreate(map, 16, 16, 0, 0);

  Allocator fov_allocator = arenaCreate(FOV_ALLOCATOR_RAM_SIZE, NULL);

  while (1) {
    fovDrawWorld(term, &map, player->hash_pos_entry.x, player->hash_pos_entry.y, fov_allocator);
    turnUser(term, &map, player);
    termDrawRefresh(term);
  }

  arenaDestroy(fov_allocator);
  arenaDestroy(arena);
  termCtxDestroy(term);
  return 0;
}
