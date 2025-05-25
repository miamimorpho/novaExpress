#ifndef MOBS_H
#define MOBS_H

#include "map.h"

#define MOBILE_NAME_LENGTH 16

typedef char MobileName[MOBILE_NAME_LENGTH];

struct Mobile {
  HASH_POS_ENTRY;
  MobileName name;
  struct UnicodeTile tile;
  struct HashPosHead container_head;
};

struct Mobile* mobCastPos(struct HashPos*);
struct Mobile* mobCreate(struct Map, int32_t, int32_t);
void mobName(struct Mobile*, MobileName);
void mobMove(struct Map*, struct Mobile*, int32_t, int32_t);
void mobPickUp(struct Mobile*, int, int);

#endif  // MOBS_H
