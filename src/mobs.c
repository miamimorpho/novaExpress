#include "mobs.h"

#include <string.h>

struct Mobile* mobCastPos(struct HashPos* pos) {
  return container_of(pos, struct Mobile, hash_pos_entry);
}

struct Mobile* mobCreate(struct Map m, int32_t x, int32_t y) {
  struct Mobile* dst = MAP_POOL_MALLOC(&m, mobs, struct Mobile, x, y);

  dst->tile.fg = 15;
  return dst;
}

void mobName(struct Mobile* mob, MobileName new_name) {
  // memset(mob->name, 0, sizeof(MobileName));
  size_t len = strlen(new_name);
  if (len >= MOBILE_NAME_LENGTH) len = MOBILE_NAME_LENGTH - 1;

  memcpy(mob->name, new_name, len);
  mob->name[len] = '\0';
}

void mobMove(struct Map* m, struct Mobile* mob_ptr, int32_t dx, int32_t dy) {
  if (!mob_ptr) return;
  struct HashPos dst = mob_ptr->hash_pos_entry;

  struct Portal* port;
  if ((port = (SHASH_GET(m->portals.hash, struct Portal, dst.x + dx,
                         dst.y + dy)))) {
    dx = (port->hash_pos_entry.x - dst.x) + dx;
    dy = (port->hash_pos_entry.y - dst.y) + dy;
  }

  struct Terra terra = terraGet(m, dst.x + dx, dst.y + dy);
  if (terra.blocks_move) return;
  /*
  struct MapLocalPos target =
      mapPosLocalise(m, dst.x + dx, dst.y +dy);
  if(mapBlocksMoveGet(target)) return;
  */

  SHASH_MOVE(m->mobs.hash, mob_ptr, dx, dy);
}

/*
struct HashPosSearch* mobOpenContainer(struct HashPosHead head){
    struct MapSearch* iter =
        VSTACK_PUSH(sizeof(struct MapSearch));
    iter->current = SLIST_FIRST(&head);
    iter->filterFn = searchAll;
    iter->archetype_heads = NULL;

    return iter;
}
*/

/*
static struct MapPos* searchFilterPos(HashPosSearch iter){
    struct MapPos filter = *(struct MapPos*)iter.filter_args;
    if( iter.cur_pos->x == filter.x &&
        iter.cur_pos->y == filter.y){
        return iter.cur_pos;
    }
    return NULL;
}

static struct MapPos* searchFilterSamePos(struct MapSearch cursor){
    struct Mobile* filter = *(struct Mobile**)cursor.filter_args;

    if(cursor.current->x == filter->pos.x &&
       cursor.current->y == filter->pos.y &&
       mobileFromPos(cursor.current) != filter)
        return cursor.current;

    return NULL;
}
*/

/*
struct Mobile* containerN(struct MapPosHead head, size_t n)
{
    struct MapPos* cursor;
    SLIST_GETN(&head, cursor, entry, n);
    return mobileFromPos(cursor);
}
*/
/*
struct MapSearch* mobSearchSamePos(struct Mobile* mob){
    struct MapSearch* iter
        = VSTACK_PUSH(sizeof(struct MapSearch));
    iter->current = SLIST_FIRST(&localisePos(mob->pos).chunk->mobile_list);
    iter->filterFn = searchFilterSamePos;
    iter->archetype_heads = NULL;
    ASSERT_SEARCH_ARGS(struct Mobile*);
    memcpy(iter->filter_args, &mob, sizeof(struct Mobile*));

    return iter;
}
*/
/*
struct MapSearch* mobSearchSingle(struct MapPos here)
{
    struct MapLocalPos here_local = localisePos(here);
    struct MapSearch* iter =
        VSTACK_PUSH(sizeof(struct MapSearch));
    iter->current = SLIST_FIRST(&here_local.chunk->mobile_list);
    iter->filterFn = searchFilterPos;
    ASSERT_SEARCH_ARGS(struct MapPos);
    memcpy(iter->filter_args, &here, sizeof(struct MapPos));

    return iter;
}

struct MapSearch* archetypeSearchRadius(struct MapPos origin,
        int32_t radius,
        ptrdiff_t archetype_head_offset)
{
    struct MapSearch* iter =
        VSTACK_PUSH(sizeof(struct MapSearch));

    iter->archetype_heads = mapGetNearbyChunks(origin, radius,
            archetype_head_offset,
            scratch_arena_);
    struct MapPosHead* head = iter->archetype_heads[0];
    iter->current = SLIST_FIRST(head);
    iter->filterFn = searchAll;

    return iter;
}
*/
/*
struct MapSearch* mobSearchRadius(struct MapPos origin,
        int32_t radius){

    ptrdiff_t offset = offsetof(struct MapChunk, mobile_list);
    return archetypeSearchRadius(origin, radius, offset);
}
*/
/*
struct MapSearch* portalSearchRadius(struct MapPos origin,
        int32_t radius){

    ptrdiff_t offset = offsetof(struct MapChunk, portal_list);
    return archetypeSearchRadius(origin, radius, offset);
}
*/
/*
struct MapPos* mapSearchNext(struct MapSearch* iter){

    if(!iter->current){
        if(iter->archetype_heads == NULL) return NULL;
        iter->archetype_heads++;
        struct MapPosHead* head = iter->archetype_heads[0];
        if(head == NULL) return NULL;

        iter->current = SLIST_FIRST(head);
        return mapSearchNext(iter);
    }

    struct MapPos* ret;
    if(!(ret = iter->filterFn(*iter) )){
       iter->current = SLIST_NEXT(iter->current, entry);
       return mapSearchNext(iter);
    }
    iter->current = SLIST_NEXT(iter->current, entry);
    return ret;
}
void mapSearchEnd(struct MapSearch* iter)
{
    VSTACK_POP(iter);

}
*/
/*
void mobMoveToContainer(struct MapPos* src, struct MapPosHead* container){

    if(src == SLIST_FIRST(container))
        return;

    struct MapLocalPos origin = localisePos(*src);
    SLIST_REMOVE(&origin.chunk->mobile_list,
                 src,
                 struct MapPos,
                 entry);
    SLIST_INSERT_HEAD(container, src, entry);
    src->where = POS_IN_CONTAINER;
    src->in_container = *container;
}

void mobPickUp(struct Mobile* mob_p, int dx, int dy){

    struct MapPos target_pos = mob_p->pos;
    target_pos.x += dx;
    target_pos.y += dy;

    struct MapSearch* search =
        mobSearchSamePos(mob_p);
    struct MapPos* target;
    if ((target = mapSearchNext(search))){
        mobMoveToContainer(target, &mob_p->inventory);
    }
    mapSearchEnd(search);
}
*/
