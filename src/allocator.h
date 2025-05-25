#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KB (1024ULL)
#define MB (1024ULL * KB)

#define container_of(ptr, type, member) \
  ((type*)((char*)(ptr) - offsetof(type, member)))

/*
 * Linear Allocator / Memory Arena
 */
typedef struct AllocatorVTable* Allocator;
typedef void* (*mallocFnPtr)(Allocator, size_t);
typedef void (*freeFnPtr)(Allocator, void*);
struct AllocatorVTable {
  mallocFnPtr mallocFn;
  freeFnPtr freeFn;
};

Allocator arenaCreate(size_t size, Allocator);
void arenaDestroy(Allocator);

/*
 * Fat Pointers
 * arrays that contain metadata of their bounds
 */
void* fatPtrCreate(size_t, size_t, Allocator);
size_t fatPtrC(void*);
void fatPtrDestroy(void*, Allocator);
#define fatPtrSize(data_ptr_) (fatPtrC(data_ptr_) * sizeof(data_ptr_[0]))

/*
 * BitMap
 * 2D 1-bit image representation
 */
typedef struct Bitmap Bitmap;
Bitmap* bitmapCreate(uint32_t, uint32_t, Allocator);
void bitmapDestroy(Bitmap*, Allocator);
void bitmapFill(Bitmap*, uint8_t);
int bitmapGetPx(Bitmap*, int32_t, int32_t);
int bitmapPutPx(Bitmap*, int32_t, int32_t, int);

#endif  // ALLOCATOR_H
