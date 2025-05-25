#include "map.h"
#include "maths.h"

struct SpatialHash {
  size_t cell_len;
  struct HashPosHead* cells;
};

struct HashPosSearch {
  struct HashPosHead* range;
  struct HashPos* cur_pos;
  size_t range_i;
  uint32_t o_x;
  uint32_t o_y;
  uint32_t max_dist;
  FilterFn filter;
  void* args;
};

union HashPosKey {
  struct {
    uint16_t x;
    uint16_t y;
  } comp;
  uint32_t key;
};

#define ASSERT_POW2(val_) (assert((val_) != 0 && ((val_) & ((val_) - 1)) == 0))

/*
 * https://nullprogram.com/blog/2018/07/31/
 * 32bit hash version 1
 */
static inline uint32_t hashFunction32(uint32_t n) {
  n ^= n >> 16;
  n *= 0x45d9f3bU;
  n ^= n >> 16;

  /* version 2
   * n ^= n >> 16;
   * n *= 0x7feb352dU;
   * n ^= n >> 15;
   * n *= 0x846ca68bU;
   * n ^= n >> 16;
   */

  // return n & (CHUNKS_MEM_C - 1);
  return n;
}

/*
 * size_scale should be a meaningful constant of the world size
 * greater cell size creates a smaller hash table
 */
struct SpatialHash* spatialHashCreate(size_t bucket_count, size_t cell_len,
                                      Allocator a) {
  ASSERT_POW2(bucket_count);
  ASSERT_POW2(cell_len);

  struct SpatialHash* p = a->mallocFn(a, sizeof(struct SpatialHash));
  p->cell_len = cell_len;
  p->cells = fatPtrCreate(bucket_count, sizeof(struct HashPosHead), a);
  for (size_t i = 0; i < bucket_count; i++) SLIST_INIT_HEAD(&p->cells[i]);
  return p;
}

void spatialHashDestroy(struct SpatialHash* hash, Allocator a) {
  a->freeFn(a, hash);
}

size_t spatialHashIndex(struct SpatialHash* hash, uint32_t x, uint32_t y) {
  union HashPosKey k = {0};
  k.comp.x = x / hash->cell_len;
  k.comp.y = y / hash->cell_len;
  size_t index = hashFunction32(k.key) MOD_POW2(fatPtrC(hash->cells));
  return index;
}

struct HashPosHead* spatialHashGetCell(struct SpatialHash* hash, uint32_t x,
                                       uint32_t y) {
  return &hash->cells[spatialHashIndex(hash, x, y)];
}

struct HashPos* spatialHashGet(struct SpatialHash* hash, uint32_t x,
                               uint32_t y) {
  assert(hash);
  struct HashPosHead* head = spatialHashGetCell(hash, x, y);
  struct HashPos* p;
  // printf("spatialHashGet %p\n", (void*)SLIST_FIRST(head));

  SLIST_FOREACH(p, head, entry) {
    // printf("%d %d \n", p->x, p->y);
    if (p->x == x && p->y == y) {
      return p;
    }
  }
  // printf("spatialHashGet %d, %d\n", x, y);
  return NULL;
}

struct HashPosSearch* spatialHashSearch(struct SpatialHash* hash, uint32_t x,
                                        uint32_t y, uint32_t r, FilterFn filter,
                                        void* args, Allocator a) {
  struct HashPosSearch* search = a->mallocFn(a, sizeof(struct HashPosSearch));

  uint32_t width = (r * 2) + 1;
  uint32_t c = width * width;
  search->range = fatPtrCreate(c, sizeof(struct HashPosHead), a);
  for (uint32_t i = 0; i < c; i++) {
    uint32_t dx = ((i % width) - r) * hash->cell_len;
    uint32_t dy = ((i / width) - r) * hash->cell_len;
    search->range[i] = *spatialHashGetCell(hash, x + dx, y + dy);
  }

  search->cur_pos = SLIST_FIRST(&search->range[0]);
  search->range_i = 0;
  search->o_x = x;
  search->o_y = y;
  search->max_dist = hash->cell_len * r;
  search->filter = filter;
  search->args = args;
  return search;
}

struct HashPos* spatialHashSearchNext(struct HashPosSearch* iter) {
  if (!iter->cur_pos) {
    if (iter->range_i == fatPtrC(iter->range) - 1) return NULL;
    iter->range_i++;
    iter->cur_pos = SLIST_FIRST(&iter->range[iter->range_i]);
    return spatialHashSearchNext(iter);
  }

  struct HashPos* ret = iter->filter(iter);
  iter->cur_pos = SLIST_NEXT(iter->cur_pos, entry);
  if (!ret ||
      chebyshevDist(ret->x, ret->y, iter->o_x, iter->o_y) >= iter->max_dist) {
    return spatialHashSearchNext(iter);
  }
  return ret;
}

void spatialHashSearchEnd(struct HashPosSearch* iter, Allocator a) {
  fatPtrDestroy(iter->range, a);
  a->freeFn(a, iter);
}

struct HashPos* searchAll(struct HashPosSearch* cursor) {
  return cursor->cur_pos;
}

void spatialHashInsert(struct SpatialHash* hash, struct HashPos* pos) {
  assert(hash);
  struct HashPosHead* head = spatialHashGetCell(hash, pos->x, pos->y);
  SLIST_INSERT_HEAD(head, pos, entry);
  // printf("inserting... %d %d\n", pos->x, pos->y);
}

void spatialHashMove(struct SpatialHash* hash, struct HashPos* pos, uint32_t dx,
                     uint32_t dy) {
  size_t src_cell = spatialHashIndex(hash, pos->x, pos->y);
  size_t dst_cell = spatialHashIndex(hash, pos->x + dx, pos->y + dy);
  if (src_cell != dst_cell) {
    SLIST_REMOVE(&hash->cells[src_cell], pos, struct HashPos, entry);
    SLIST_INSERT_HEAD(&hash->cells[dst_cell], pos, entry);
  }
  pos->x += dx;
  pos->y += dy;
}

void spatialHashRemove(struct SpatialHash* hash, struct HashPos* pos) {
  assert(hash);
  struct HashPosHead* head = spatialHashGetCell(hash, pos->x, pos->y);
  SLIST_REMOVE(head, pos, struct HashPos, entry);
}
struct MapPool mapPoolCreate(size_t bucket_count, size_t cell_len,
                             Allocator allocator) {
  struct MapPool ret = {0};
  SLIST_INIT_HEAD(&ret.free_list);
  ret.hash = spatialHashCreate(bucket_count, cell_len, allocator);
  return ret;
}

struct Map mapCreate(Allocator allocator) {
  struct Map ret = {0};
  ret.arena = allocator;
  ret.portals = mapPoolCreate(4, 1, allocator);
  ret.chunks = mapPoolCreate(4, 1, allocator);
  ret.mobs = mapPoolCreate(32, 16, allocator);

  return ret;
}

void* mapPoolMallocFn(struct MapPool* pool, size_t elm_size,
                      size_t hash_entry_offset, int64_t x, int64_t y,
                      Allocator a) {
  void* ret = NULL;
  struct HashPos* pos = SLIST_FIRST(&pool->free_list);
  if (pos == NULL) {
    ret = a->mallocFn(a, elm_size);
    pos = (struct HashPos*)((char*)ret + hash_entry_offset);
    // printf("malloc...\n");
  } else {
    ret = (char*)pos - hash_entry_offset;
    SLIST_REMOVE_HEAD(&pool->free_list, entry);
  }
  pos->x = (x);
  pos->y = (y);
  spatialHashInsert(pool->hash, pos);

  return ret;
}

