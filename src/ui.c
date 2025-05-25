#include "ui.h"

/*
struct TermInputComm termButton(TermCtx term, const char* str) {
  struct TermInputComm in = termGetInput(term);
  ivec2 start = term->cursor;
  term->fg = 3;
  term->bg = 1;
  termPrint(term, str);
  uint32_t width = 8;
  uint32_t height = 1;

  uint32_t local_x = (uint32_t)(in.mouse_x * ASCII_SCREEN_WIDTH) - start.x;
  uint32_t local_y = (uint32_t)(in.mouse_y * ASCII_SCREEN_HEIGHT) - start.y;
  uint32_t local_press = 0;

  // printf("%u %u\n", local_x, local_y);
  if (local_x <= width && local_x >= 0 && local_y < height && local_y >= 0) {
    // printf("termbutton\n");
    termMove(term, start.x, start.y);
    term->fg = 1;
    term->bg = 3;
    termPrint(term, str);
    // todo, draw in rest of size
    local_press = IS_MOUSE_PRESSED(in.unicode);
  }
  termPrint(term, "\n");

  return (struct TermInputComm){
      .unicode = local_press,
      .mouse_x = local_x,
      .mouse_y = local_y,
  };
}
*/
/*

int menuContainerEntries(TermCtx term, struct HashPosHead container){

    termMove(term, 0, 0);
    termFg(term, 15); termBg(term, 1);
    termPrint(term, "inventory\n");

    struct Mobile* mob;
    struct SpatialHashSearch* search =
        mobOpenContainer(container);
    while((mob = mobileFromPos(mapSearchNext(search)))){
        if(termButton(term, mob->name).unicode)
            goto something;
    }
    mapSearchEnd(search);

    if(termButton(term, "exit").unicode)
        goto something;

    return 0;

    something:
    mapSearchEnd(search);
    return 1;
}

*/
