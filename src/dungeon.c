#include "dungeon.h"

#include "wfc.h"

void dungeonBuild(struct Map* m) {
  struct wfc_image* sample = wfc_img_load("textures/wfctest.png");
  struct wfc* wfc = wfc_overlapping(32, 32, sample, 3, 3, 1, 1, 1, 1);
  wfc_run(wfc, -1);

  struct wfc_image* output = wfc_output_image(wfc);

  struct Terra terra = {
      .tile =
          (struct UnicodeTile){
              363,
              2,
              8,
              0,
          },
      .blocks_view = 1,
      .blocks_move = 1,
  };

  for (int y = 0; y < output->height; y++) {
    for (int x = 0; x < output->width; x++) {
      size_t i = (y * output->width + x) * output->component_cnt;
      if (output->data[i] == 0) {
        terraPut(m, x, y, terra);
      }
    }
  }

  wfc_export(wfc, "output_test.png");
  wfc_img_destroy(sample);
  wfc_destroy(wfc);
}
