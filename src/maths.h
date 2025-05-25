#ifndef MATHS_H
#define MATHS_H

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
typedef struct {
  float x;
  float y;
} vec2;
*/

/*
typedef struct {
  int32_t x;
  int32_t y;
} ivec2;
*/

/*
typedef struct {
  int32_t x;
  int32_t y;
  int32_t z;
} ivec3;
*/
/*
 * Power of two function 0, 1, 2, 4, 8, 16, 32...
 */

/* when n = 0, return = 1,
 * 1, 2
 * 2, 4
 * 3, 8
 * ...
 */
static inline unsigned long long pow2(int n) {
  assert(n >= 0 && n <= 63);
  // if (n < 0 || n > 63)
  return 1ULL << n;
}

// Fast modulo, if number is power of two
#define MOD_POW2(power_of_two_) &((power_of_two_) - 1)

/*
 * n is number we want to round
 * p is a power of two
 * this function rounds n down to the nearest multiple of p
 */
static inline int64_t alignDown(int64_t n, int64_t p) { return n & ~(p - 1); }

static inline int64_t alignUp(int64_t n, int64_t p) {
  int64_t mask = p - 1;
  return (n + mask) & ~mask;
}

static inline int iMax(int a, int b) { return (a > b) ? a : b; }

static inline int iMin(int a, int b) { return (a < b) ? a : b; }

/* Distance Functions
 *
 */
static inline unsigned long relativeDist(long x1, long y1, long x2, long y2) {
  unsigned long dx = labs(x2 - x1);
  unsigned long dy = labs(y2 - y1);
  return (dx * dx) + (dy * dy);
}

static inline unsigned long chebyshevDist(long x1, long y1, long x2, long y2) {
  return iMax(labs(y2 - y1), labs(x2 - x1));
}

static inline unsigned long manhattanDist(long x1, long y1, long x2, long y2) {
  return labs(y2 - y1) + labs(x2 - x1);
}

/* Fractions
 */
typedef struct {
  long long num;
  long long den;
} Fraction;

#define FR_MUL(val_, fr_) (Fraction){(((fr_).num) * (val_)), (fr_).den}

static inline double frResolve(Fraction fr) {
  assert(fr.den);
  return (double)fr.num / (double)fr.den;
}

static inline int frRoundTiesUp(Fraction fr) {
  return floor(frResolve(fr) + 0.5);
}

static inline int frRoundTiesDown(Fraction fr) {
  return ceil(frResolve(fr) - 0.5);
}

static inline bool frCompare(Fraction small, Fraction large) {
  return (small.num * large.den <= large.num * small.den);
}

// Packer Functions
static inline uint8_t pack_uint4(uint8_t a, uint8_t b) {
  return (a & 0x0F) | ((b & 0x0F) << 4);
}


static inline uint64_t hashFunction64(uint64_t n) {
  /*
   * https://code.google.com/archive/p/fast-hash/
   * 64bit hash
   */
  n ^= n >> 23;
  n *= 0x2127599bf4325c37ull;
  n ^= n >> 47;

  // return n & (CHUNKS_MEM_C - 1);
  return n;
}


#endif  // MATHS_H
