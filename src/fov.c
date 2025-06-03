#include "fov.h"

#include <stdlib.h>

#include "maths.h"

// todo draw the entire map into the tile indices buffer
// use the visability buffer to dim/null out unseen tiles

enum { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 };

struct Row {
  struct MapPos camera;
  int cardinal;
  int depth;
  Fraction start_slope;
  Fraction end_slope;
};

struct RenderClosure {
  struct ShadowcastVTable_ vtable_;
  TermCtx term;
  int dx;
  int dy;
};

void shadowcastRenderTile(ShadowcastVTable vtable_ptr, uint32_t x, uint32_t y,
                          struct UnicodeTile t) {
  struct RenderClosure* closure =
      container_of(vtable_ptr, struct RenderClosure, vtable_);

  int scr_x = x - closure->dx;
  int scr_y = y - closure->dy;
  closure->term->atlas = t.atlas;
  closure->term->fg = t.fg;
  closure->term->bg = t.bg;
  tileMvAdd(closure->term, scr_x, scr_y, t.unicode);
}

/* turns a shadowcast coordinate (row_depth,row_col)
 * that is relative to a single origin point into
 * a chunk-local position (x, y) we can use to query
 * the world arena
 */
static void shadowPosToMapPos(int cardinal, int depth, int col,
                              struct MapPos camera, struct MapPos* out) {
  switch (cardinal) {
    case NORTH:
      out->x = camera.x + col;
      out->y = camera.y - depth;
      break;
    case SOUTH:
      out->x = camera.x + col;
      out->y = camera.y + depth;
      break;
    case EAST:
      out->x = camera.x + depth;
      out->y = camera.y + col;
      break;
    case WEST:
      out->x = camera.x - depth;
      out->y = camera.y + col;
      break;
  }
}

/* reverse of shadowmaskToWorldspace - which is quadratic
 * if second_solution is given false, first answer is given
 *   if a second answer exists, second_solution is set true
 * if second_solution is given true, second answer is given
 *   and second_solution is set to false
 */
void worldPosToShadowPos(struct MapPos in, struct MapPos camera,
                         int* cardinal_out, int* depth_out, int* col_out,
                         bool* second_solution) {
  int dx = in.x - camera.x;
  int dy = in.y - camera.y;

  bool is_diagonal = abs(dx) == abs(dy);

  if (*second_solution ^ (abs(dx) >= abs(dy))) {
    // East or West
    *col_out = dy;
    *depth_out = abs(dx);
    *cardinal_out = (dx > 0) ? EAST : WEST;
  } else {
    // North or South
    *col_out = dx;
    *depth_out = abs(dy);
    *cardinal_out = (dy > 0) ? SOUTH : NORTH;
  }

  *second_solution = (is_diagonal && !*second_solution);
}

static Fraction slope(int row_depth, int col) {
  // assert(row_depth); // divide by 0 error
  if (!row_depth) return (Fraction){0, 1};

  return (Fraction){2 * col - 1, 2 * row_depth};
}

/* returns true if the given position (col, row.depth)
 * modelled as a central point in tile space
 * is within the racycast arc
 */
static bool isSymmetric(struct Row* row, int col) {
  return (col >= frResolve(FR_MUL(row->depth, row->start_slope)) &&
          col <= frResolve(FR_MUL(row->depth, row->end_slope)));
}
// replace maths functions with actual code and document here, very specific stuff
// going on here

static void shadowcastScanRow(struct Row cur_row, Bitmap* dst_mask,
                              ShadowcastVTable vtable) {
  static float SHADOWCAST_DEPTH_MAX = 12.5;
  if (cur_row.depth > SHADOWCAST_DEPTH_MAX) return;

  if(frCompare(cur_row.end_slope, cur_row.start_slope)) return;

  // bounds each cast between two diamonds on a tiled grid
  int min_col =
      frRoundTiesUp(FR_MUL(cur_row.depth, cur_row.start_slope));
  int max_col =
      frRoundTiesDown(FR_MUL(cur_row.depth, cur_row.end_slope));

  bool prev_was_wall = false;
  for (int col = min_col; col <= max_col; col++) {
    struct MapPos cur_pos = cur_row.camera;
    shadowPosToMapPos(cur_row.cardinal, cur_row.depth, col, cur_row.camera,
                      &cur_pos);

    struct Terra terra = terraGet(cur_pos.map, cur_pos.x, cur_pos.y);
    uint8_t is_wall = terra.blocks_move;
    if (!is_wall || isSymmetric(&cur_row, col)) {
      vtable->renderTile(vtable, cur_pos.x, cur_pos.y, terra.tile);
      bitmapPutPx(dst_mask, cur_pos.x, cur_pos.y, 1);
    } 
    if (prev_was_wall && !is_wall) {
      cur_row.start_slope = slope(cur_row.depth, col);
    }
    if (!prev_was_wall && is_wall) {
      struct Row next_row = cur_row;
      next_row.depth += 1;
      next_row.end_slope = slope(cur_row.depth, col); 
      shadowcastScanRow(next_row, dst_mask, vtable);
    }
    prev_was_wall = is_wall;

  }  // end of row scanning

  if (!prev_was_wall) {
    struct Row next_row = cur_row;
    next_row.depth += 1;
    shadowcastScanRow(next_row, dst_mask, vtable);
  }

  return;
}

static void fovShadowcast(struct Map* m, uint32_t x_in, uint32_t y_in,
                          Bitmap* dst_mask, ShadowcastVTable effect) {
  struct Terra t = terraGet(m, x_in, y_in);
  effect->renderTile(effect, x_in, y_in, t.tile);
  bitmapPutPx(dst_mask, x_in, y_in, 1);
  for (int cardinal = 0; cardinal < 4; cardinal++) {
    struct Row first_row = {.camera = (struct MapPos){m, x_in, y_in},
                            .cardinal = cardinal,
                            .depth = 1,
                            .start_slope = (Fraction){-1, 1},
                            .end_slope = (Fraction){1, 1}};
    shadowcastScanRow(first_row, dst_mask, effect);
  }
}

void fovPortal(struct MapPos camera, struct Portal portal, Bitmap* dst_mask,
               ShadowcastVTable effect) {
  struct MapPos src = MAP_POS_INIT(camera.map, portal.hash_pos_entry.x,
                                   portal.hash_pos_entry.y);
  struct MapPos dst = MAP_POS_INIT(camera.map, portal.dst_x, portal.dst_y);
  struct Terra t = terraGet(camera.map, portal.dst_x, portal.dst_y);
  effect->renderTile(effect, dst.x, dst.y, t.tile);
  bitmapFill(dst_mask, 0);

  bool second_solution = false;
  do {
    int cardinal, depth, col;
    worldPosToShadowPos(src, camera, &cardinal, &depth, &col, &second_solution);
    struct Row first_row = {
        .camera = dst,
        .cardinal = cardinal,
        .depth = 1,
        .start_slope = slope(depth, col),
        .end_slope = slope(depth, col + 1),
    };
    shadowcastScanRow(first_row, dst_mask, effect);
  } while (second_solution);
}

int fovDrawWorld(TermCtx term, struct Map* m, uint32_t x_in, uint32_t y_in,
                 Allocator allocator) {
  int scr_width = TILE_BUFFER_WIDTH;//termGetScreenWidth(term);
  int scr_height = TILE_BUFFER_WIDTH;//termGetScreenHeight(term);

  int dx = x_in - (TILE_BUFFER_WIDTH / 2);
  int dy = y_in - (TILE_BUFFER_WIDTH / 2);

  for(int y = 0; y < TILE_BUFFER_WIDTH; y++){
    for(int x = 0; x < TILE_BUFFER_WIDTH; x++){
      struct Terra t = terraGet(m, dx + x, dy +y);
      tileMvAdd(term, x, y, t.tile.unicode); 
    }
  }
 
  struct RenderClosure closure = {
      .term = term,
      .dx = dx,
      .dy = dy,
      .vtable_ = (struct ShadowcastVTable_){shadowcastRenderTile}};
  ShadowcastVTable effects = &closure.vtable_;

  /* First chunk rendering pass
   * draws terrain to2 the screen and a visibility mask
   * bmp, this can be used to occlude sparse object data
   * on following passes.
   */
  Bitmap* mask = bitmapCreate(scr_width, scr_height, allocator);
  fovShadowcast(m, x_in, y_in, mask, effects);
  //termDrawPushZ(term);

  struct Mobile* mob;
  struct HashPosSearch* iter = spatialHashSearch(m->mobs.hash, x_in, y_in, 1,
                                                 &searchAll, NULL, allocator);
  while ((mob = MAP_CAST(struct Mobile, spatialHashSearchNext(iter)))) {
    effects->renderTile(effects, mob->hash_pos_entry.x, mob->hash_pos_entry.y,
                        mob->tile);
  }
  spatialHashSearchEnd(iter, allocator);
  //termDrawPushZ(term);

 
  /*
  struct Portal* portal;
  iter = MAP_SEARCH(camera, portals, 1, allocator);
  while((portal = MAP_CAST(struct Portal, spatialHashSearchNext(iter)))){
      if (bitmapGetPx(mask, portal->src.x, portal->src.y) == 1){
          closure.dx = dx + portal->dst_x - portal->hash_pos_entry.x;
          closure.dy = dy + portal->dst_y - portal->hash_pos_entry.y;
          fovPortal(camera, *portal, mask, effects);
      }

  }
  */
  
  // spatialHashSearchEnd(iter, allocator);
  //termDrawPushZ(term);

  bitmapDestroy(mask, allocator);
  return 0;
}
