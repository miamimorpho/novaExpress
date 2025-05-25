#include "mobs.h"
#include "ui.h"
#include "bios.h"

int turnUser(TermCtx term, struct Map* map, struct Mobile* mob) {
  struct TermInputComm in = termGetInput(term);

  if (IS_MOUSE_PRESSED(in.unicode)) {
    int dx = 0;
    int dy = 0;
    if (in.mouse_x > 0.55) dx = 1;
    if (in.mouse_x < 0.45) dx = -1;

    if (in.mouse_y > 0.55) dy = 1;
    if (in.mouse_y < 0.45) dy = -1;

    mobMove(map, mob, dx, dy);
    return 1;
  }

  switch (in.unicode) {
    case 'w':
      mobMove(map, mob, 0, -1);
      break;
    case 'a':
      mobMove(map, mob, -1, 0);
      break;
    case 's':
      mobMove(map, mob, 0, 1);
      break;
    case 'd':
      mobMove(map, mob, 1, 0);
      break;
    case 'p':
      // mobPickUp(mob, 0, 0);
      break;
    case 'i':
      // TERM_WAIT(term, menuContainerEntries(term, mob->inventory));
      break;
    default:
      return 0;
  }

  return 1;
}
