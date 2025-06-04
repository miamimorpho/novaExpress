#ifndef WORLD_H
#define WORLD_H

#include <assert.h>
#include <stdint.h>

#include "allocator.h"
#include "slist.h"

#define CHUNK_ALLOCATOR_RAM_SIZE (256 * KB)
#define MAP_SCALE 8
#define CHUNK_LEN 16
#define CHUNK_AREA CHUNK_LEN* CHUNK_LEN

typedef struct SpatialHash SpatialHash;
struct HashPos {
  SLIST_ENTRY(struct HashPos) entry;
  uint32_t x;
  uint32_t y;
};
#define HASH_POS_ENTRY struct HashPos hash_pos_entry
#define HASH_POS_INIT(x_, y_) \
  (struct HashPos) { NULL, (x_), (y_) }

SLIST_HEAD(HashPosHead, struct HashPos);
struct MapPool {
  struct HashPosHead free_list;
  SpatialHash* hash;
};

struct Map {
  Allocator arena;
  struct MapPool chunks;
  struct MapPool portals;
  struct MapPool mobs;
};

struct MapPos {
  struct Map* map;
  uint32_t x;
  uint32_t y;
};
#define MAP_POS_INIT(m, x, y) ((struct MapPos){m, x, y})

struct UnicodeTile {
  uint32_t unicode;
  uint8_t atlas;
  uint8_t fg;
  uint8_t bg;
};
#define UNPACK_UNICODE(u_) (u_)->unicode, (u_)->atlas, (u_)->fg, (u_)->bg

struct Terra {
  struct UnicodeTile tile;
  unsigned int blocks_view : 1;
  unsigned int blocks_move : 1;
};

struct Portal {
  HASH_POS_ENTRY;
  uint32_t dst_y;
  uint32_t dst_x;
};

/*
 * Spatial Hash
 * hash table that uses 2D positions as keys
 */

typedef struct HashPosSearch HashPosSearch;
typedef struct HashPos* (*FilterFn)(HashPosSearch*);

SpatialHash* spatialHashCreate(size_t, size_t, Allocator);
void spatialHashDestroy(SpatialHash*, Allocator a);

struct HashPos* spatialHashGet(SpatialHash*, uint32_t, uint32_t);
void spatialHashInsert(SpatialHash*, struct HashPos*);
void spatialHashMove(SpatialHash*, struct HashPos*, uint32_t, uint32_t);
void spatialHashRemove(SpatialHash*, struct HashPos*);

#define SHASH_GET(hash_, type_, x_, y_) \
  (container_of(spatialHashGet((hash_), (x_), (y_)), type_, hash_pos_entry))
#define SHASH_INSERT(hash_, elm_) \
  (spatialHashInsert((hash_), &(elm_)->hash_pos_entry))
#define SHASH_REMOVE(hash_, elm_) \
  (spatialHashRemove((hash_), &(elm_)->hash_pos_entry))
#define SHASH_MOVE(hash_, elm_, dx_, dy_) \
  (spatialHashMove((hash_), &(elm_)->hash_pos_entry, (dx_), (dy_)))

HashPosSearch* spatialHashSearch(SpatialHash*, uint32_t, uint32_t, uint32_t r,
                                 FilterFn, void*, Allocator);
struct HashPos* spatialHashSearchNext(HashPosSearch*);
void spatialHashSearchEnd(HashPosSearch* iter, Allocator a);
struct HashPos* searchAll(struct HashPosSearch* cursor);

#define MAP_POOL_MALLOC(map_, entry_, type_, x_, y_)            \
  (mapPoolMallocFn(&(map_)->entry_, sizeof(type_),              \
                   offsetof(type_, hash_pos_entry), (x_), (y_), \
                   (map_)->arena))

void* mapPoolMallocFn(struct MapPool*, size_t, size_t, int64_t, int64_t,
                      Allocator);

#define MAP_SEARCH(map_, entry_, radius_, allocator_)                        \
  (spatialHashSearch((pos_).map->entry_.hash, (pos_).x, (pos_).y, (radius_), \
                     &searchAll, (NULL), allocator))
/* Terrain.c */

#define MAP_CAST(parent_type_, hash_pos_) \
  (container_of((hash_pos_), parent_type_, hash_pos_entry))

struct Map mapCreate(Allocator allocator);
struct MapChunk* mapChunkInsert(struct Map* m, uint32_t, uint32_t);

struct Terra terraGet(struct Map*, uint32_t, uint32_t);
void terraPut(struct Map*, uint32_t, uint32_t, struct Terra);

struct Portal* portalCreate(struct Map, uint32_t, uint32_t, uint32_t, uint32_t);
struct Portal* portalGet(struct Map m, uint32_t, uint32_t);

#endif  // WORLD_H
