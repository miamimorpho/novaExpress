#ifndef SLIST_H_
#define SLIST_H_

#define SLIST_HEAD(name, type) \
  struct name {                \
    type *first;               \
  }
#define SLIST_HEAD_INITIALIZER(name) \
  (name) { NULL }

#define SLIST_ENTRY(type) \
  struct {                \
    type *next;           \
  }

#define SLIST_FIRST(head) ((head)->first)
#define SLIST_END(head) NULL
#define SLIST_EMPTY(head) (SLIST_FIRST(head) == SLIST_END(head))
#define SLIST_NEXT(elm, field) ((elm)->field.next)

#define SLIST_INIT_HEAD(head)            \
  do {                                   \
    SLIST_FIRST(head) = SLIST_END(head); \
  } while (0)

/* var is pointer of SLIST_ENTRY's type
 * head is a pointer to SLIST_HEAD
 * field is the struct member for next SLIST_ENTRY
 */
#define SLIST_FOREACH(var, head, field)                     \
  for ((var) = SLIST_FIRST(head); (var) != SLIST_END(head); \
       (var) = SLIST_NEXT(var, field))

#define SLIST_FOREACH_SAFE(var, head, field, tvar) \
  for ((var) = SLIST_FIRST(head);                  \
       (var) && ((tvar) = SLIST_NEXT(var, field), 1); (var) = (tvar))

#define SLIST_INSERT_HEAD(head, elm, field) \
  do {                                      \
    (elm)->field.next = (head)->first;      \
    (head)->first = (elm);                  \
  } while (0)

#define SLIST_REMOVE_HEAD(head, field)         \
  do {                                         \
    (head)->first = (head)->first->field.next; \
  } while (0)

#define SLIST_INSERT_AFTER(dst, src, field)        \
  do {                                             \
    (src)->field.sle_next = (dst)->field.sle_next; \
    (dst)->field.sle_next = (src);                 \
  } while (0)

#define SLIST_REMOVE_AFTER(elm, field)                 \
  do {                                                 \
    (elm)->field.next = (elm)->field.next->field.next; \
  } while (0)

#define SLIST_REMOVE(head, elm, type, field)                           \
  do {                                                                 \
    if ((head)->first == (elm)) {                                      \
      SLIST_REMOVE_HEAD((head), field);                                \
    } else {                                                           \
      type *curelm = (head)->first;                                    \
      while (curelm->field.next != (elm)) curelm = curelm->field.next; \
      curelm->field.next = (elm)->field.next;                          \
    }                                                                  \
  } while (0)

#define SLIST_GETN(head, elm, field, n)                          \
  do {                                                           \
    (elm) = SLIST_FIRST(head);                                   \
    for (size_t i = 0; (elm) != SLIST_END(head) && i < (n); i++) \
      elm = SLIST_NEXT((elm), field);                            \
  } while (0)

#endif  // SLIST_H_
