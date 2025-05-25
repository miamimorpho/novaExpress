#include "allocator.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maths.h"

#define UINT32_DEADBEEF 0xDEADBEEF

static const ptrdiff_t ARCH_ALIGNMENT = 2 * sizeof(void*);

/* Heap based stack allocator */
struct Arena {
  struct AllocatorVTable vtable;
  char* beg;
  char* offset;
  char* end;
};

static inline struct Arena* arenaHeader(Allocator allocator_ptr) {
  return container_of(allocator_ptr, struct Arena, vtable);
}

static void* arenaSuballoc(Allocator allocator, size_t size) {
  struct Arena* a = arenaHeader(allocator);
  assert(a);

  ptrdiff_t padding = -(uintptr_t)a->offset MOD_POW2(ARCH_ALIGNMENT);
  // derived from 'padding = addr % align' assuming ARCH_ALIGNMENT is always a
  // power of 2
  size_t available = a->end - a->offset - padding;
  if (available < size) {
    fprintf(stderr,
            "ERR memArenaSuballoc: tried allocating %zu only %zu left!\n", size,
            available);
    abort();
  }

  void* p = (char*)((uintptr_t)a->offset + padding);
  a->offset += padding + size;
  return memset(p, 0, size);
}

static void arenaPop(Allocator allocator, void* allocation) {
  struct Arena* a = arenaHeader(allocator);

  assert(a);
  char* p_offset = (char*)allocation;
  if (p_offset < a->end && p_offset >= a->beg) {
    ptrdiff_t size = a->offset - p_offset;
    a->offset = p_offset;
    memset(a->offset, 0, size);
  } else {
    printf("memArenaPop fail\n");
  }
}

Allocator arenaCreate(size_t size, Allocator host) {
  struct Arena* arena = NULL;
  if (!host) {
    arena = malloc(sizeof(struct Arena) + size);
  } else {
    arena = host->mallocFn(host, sizeof(struct Arena) + size);
  }

  if (!arena) {
    fprintf(stderr, "not enough RAM to allocate MemArena\n");
    abort();
  }

  arena->vtable = (struct AllocatorVTable){arenaSuballoc, arenaPop};
  arena->beg = (char*)arena + sizeof(struct Arena);
  arena->offset = arena->beg;
  arena->end = arena->offset + size;

  return &arena->vtable;
}

void arenaDestroy(Allocator a) {
  struct Arena* arena = arenaHeader(a);
  free(arena);
}

/*
 * FatPtr is an array-type stores its size and is backed by an allocator
 * a sentinal value is at the start of the structure to detect memory
 * corruption that could have leaked into this array
 */
struct FatPtr {
  uint32_t sentinal;
  size_t count;
  char data[];
};

/*
 * by using container_of() we can keep the calling scopes typing of
 * the array by hiding info behind the data pointer, this tecnique
 * also maintains alignment of data[] assuming its container is aligned.
 * A sentinal is checked for integrity, this indicates if the array
 * was corrupted or this function was used on a non-slice pointer.
 */
static inline struct FatPtr* fatPtrHeader(void* data_ptr) {
  struct FatPtr* s = container_of(data_ptr, struct FatPtr, data);
  if (s->sentinal != UINT32_DEADBEEF) {
    printf("error, using unintialised memory, segfault, fatPtrHeader\n");
    abort();
  }
  return s;
}
size_t fatPtrC(void* data_ptr) { return fatPtrHeader(data_ptr)->count; }

void* fatPtrCreate(size_t c, size_t stride, Allocator allocator) {

  assert(allocator);

  if (stride && c > SIZE_MAX / stride) {
    fprintf(stderr, "count %zu stride %zu\n", c, stride);
    fprintf(stderr, "FatPtrCreate: integer overflow\n");
    return NULL;
  }
  size_t total_size = sizeof(struct FatPtr) + c * stride;

  struct FatPtr* s = allocator->mallocFn(allocator, total_size);
  s->sentinal = UINT32_DEADBEEF;
  s->count = c;
  memset(s->data, 0, c * stride);
  return s->data;
}

void fatPtrDestroy(void* s_ptr, Allocator allocator) {
  struct FatPtr* s = fatPtrHeader(s_ptr);
  allocator->freeFn(allocator, s);
}

/*
 * Bitmaps
 * a 1-bit image datatype
 */
struct Bitmap {
  uint32_t width;
  uint32_t height;
  char data[];
};

#define BITMASK_AT(offset) (1 << (7 - (offset)))

static inline uint32_t roundBitUp(uint32_t bit_index) {
  // add 7, clear last three bits, rounding down
  return (bit_index + 7) & ~7;
}

static inline uint8_t getBitInByte(uint8_t offset, uint8_t byte) {
  return byte & BITMASK_AT(offset) ? 1 : 0;
}

static inline size_t bitmapDataSize(uint32_t width, uint32_t height) {
  return (roundBitUp(width) * roundBitUp(height)) / 8;
}

void bitmapFill(struct Bitmap* bmp, uint8_t val) {
  if (!bmp) return;
  if (val) val = 1;

  size_t size = bitmapDataSize(bmp->width, bmp->height);
  memset(bmp->data, val, size);
}

struct Bitmap* bitmapCreate(uint32_t width, uint32_t height,
                            Allocator allocator) {
  size_t total_size = sizeof(Bitmap) + bitmapDataSize(width, height);
  ;

  Bitmap* bmp = allocator->mallocFn(allocator, total_size);
  bmp->width = width;
  bmp->height = height;
  bitmapFill(bmp, 0);
  return bmp;
}

void bitmapDestroy(Bitmap* bmp, Allocator allocator) {
  allocator->freeFn(allocator, bmp);
}

int bitmapGetPx(struct Bitmap* bmp, int32_t x, int32_t y) {
  if (!bmp || x < 0 || x >= (int)bmp->width || y < 0 || y >= (int)bmp->height)
    return 1;

  int bit_index = y * bmp->width + x;
  int offset = bit_index % 8;
  uint8_t byte = bmp->data[bit_index / 8];
  return getBitInByte(offset, byte);
}

int bitmapPutPx(struct Bitmap* bmp, int32_t x, int32_t y, int val) {
  if (!bmp || x < 0 || x >= (int)bmp->width || y < 0 || y >= (int)bmp->height)
    return 1;

  int bit_index = y * bmp->width + x;
  int offset = bit_index % 8;
  char* dst = &bmp->data[bit_index / 8];

  if (val)
    *dst |= BITMASK_AT(offset);
  else
    *dst &= ~BITMASK_AT(offset);

  return 0;
}

