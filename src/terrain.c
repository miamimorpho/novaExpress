#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map.h"

struct MapChunk {
  HASH_POS_ENTRY;

  struct UnicodeTile* tiles;
  Bitmap* blocks_view_bmp;
  Bitmap* blocks_move_bmp;
};

struct UnicodeTile NULL_TILE = {
    .unicode = 370,
    .atlas = 2,
    .fg = 15,
    .bg = 0,
};

int mapChunkFillPaint(struct MapChunk chunk, struct UnicodeTile val) {
  for (size_t i = 0; i < CHUNK_AREA; i++) chunk.tiles[i] = val;
  return 0;
}

struct MapChunk* mapChunkInsert(struct Map* m, uint32_t x, uint32_t y) {
  if (spatialHashGet(m->chunks.hash, x, y)) {
    printf("chunk already exists here");
    return NULL;
  }

  struct MapChunk* ret;

  if (!(ret = MAP_POOL_MALLOC(m, chunks, struct MapChunk, x, y))) {
    printf("out of memory for chunks\n");
    return NULL;
  }

  ret->blocks_view_bmp = bitmapCreate(CHUNK_LEN, CHUNK_LEN, m->arena);
  ret->blocks_move_bmp = bitmapCreate(CHUNK_LEN, CHUNK_LEN, m->arena);
  ret->tiles = fatPtrCreate(CHUNK_AREA, sizeof(struct Terra), m->arena);

  struct UnicodeTile air = {
      .unicode = 0,
      .atlas = 2,
      .fg = 15,
      .bg = 3,
  };

  mapChunkFillPaint(*ret, air);
  return ret;
}

void mapDestroy(struct Map m) { spatialHashDestroy(m.chunks.hash, m.arena); }

/*
struct TerraPos terrainLocalisePos(struct Map m, uint32_t x, uint32_t y )
{
    return (struct TerraPos){
        .x = x % CHUNK_LEN,
        .y = y % CHUNK_LEN,
        .chunk = SHASH_GET(m.chunks.hash, struct MapChunk,
                x / CHUNK_LEN, y / CHUNK_LEN)
    };
}
*/

struct Terra terraGet(struct Map* m, uint32_t x_in, uint32_t y_in) {
  // localise coordinates to chunk
  struct MapChunk* chunk = SHASH_GET(m->chunks.hash, struct MapChunk,
                                     x_in / CHUNK_LEN, y_in / CHUNK_LEN);
  if (!chunk) return (struct Terra){NULL_TILE, 0, 0};
  uint32_t x = x_in % CHUNK_LEN;
  uint32_t y = y_in % CHUNK_LEN;

  uint32_t offset = (y * CHUNK_LEN) + x;

  return (struct Terra){
      .tile = chunk->tiles[offset],
      .blocks_view = bitmapGetPx(chunk->blocks_view_bmp, x, y),
      .blocks_move = bitmapGetPx(chunk->blocks_move_bmp, x, y),
  };
}

void terraPut(struct Map* m, uint32_t x_in, uint32_t y_in, struct Terra put) {
  // printf("terrput %d %d\n", x_in, y_in);

  // localise coordinates to chunk
  struct MapChunk* chunk = SHASH_GET(m->chunks.hash, struct MapChunk,
                                     x_in / CHUNK_LEN, y_in / CHUNK_LEN);
  if (!chunk) {
    // printf("creating chunk...\n");
    if (!(chunk = mapChunkInsert(m, x_in / CHUNK_LEN, y_in / CHUNK_LEN)))
      abort();
  }

  uint32_t x = x_in % CHUNK_LEN;
  uint32_t y = y_in % CHUNK_LEN;
  uint32_t offset = (y * CHUNK_LEN) + x;

  chunk->tiles[offset] = put.tile;
  bitmapPutPx(chunk->blocks_view_bmp, x, y, put.blocks_view);
  bitmapPutPx(chunk->blocks_move_bmp, x, y, put.blocks_move);
}

/*
char TerraBlocksViewGet(struct TerraPos p){
    return bitmapGetPx(p.chunk->blocks_view_bmp, p.x, p.y);
}

void TerraBlocksViewPut(struct TerraPos p, char val){
    bitmapPutPx(p.chunk->blocks_view_bmp, p.x, p.y, val);
}

char TerraBlocksMoveGet(struct TerraPos p){
    return bitmapGetPx(p.chunk->blocks_move_bmp, p.x, p.y);

}
void TerraBlocksMovePut(struct TerraPos p, char val){
    bitmapPutPx(p.chunk->blocks_move_bmp, p.x, p.y, val);
}

struct UnicodeTile TerraTileGet(struct TerraPos p){
    if(!p.chunk)
        return NULL_TILE;

    uint32_t offset = (p.y * CHUNK_LEN) + p.x;
    return p.chunk->tiles[offset];
}

void TerraTilePut(struct TerraPos p, struct UnicodeTile t){
    if(!p.chunk)
        return;

    uint32_t offset = (p.y * CHUNK_LEN) + p.x;
    p.chunk->tiles[offset] = t;
}
*/
struct Portal* portalCreate(struct Map m,
                            uint32_t src_x, uint32_t src_y,
                            uint32_t dst_x, uint32_t dst_y)
{
    if(spatialHashGet(m.portals.hash, src_x, src_y)) return NULL;

    struct Portal* port =
        MAP_POOL_MALLOC(&m, portals, struct Portal, src_x, src_y);

    port->dst_x = dst_x;
    port->dst_y = dst_y;
    return port;
}

