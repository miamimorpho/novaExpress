/*
 * Wave Function Collapse: C Implementation
 * Original Author: Krystian Samp
 * Modifications by: Naomi Winter
 * License: MIT (see full license in license/WFC_MIT_LICENSE.txt)
 */

// Usage:
//
//         struct wfc *wfc = wfc_overlapping(
//             128,             // Output image width in pixels
//             128,             // Output image height in pixels
//             input_image,     // Input image that will be cut into tiles
//             3,               // Tile width in pixels
//             3,               // Tile height in pixels
//             1,               // Expand input image on the right and bottom
//             1,               // Add horizontal flips of all tiles
//             1,               // Add vertical flips of all tiles
//             1                // Add n*90deg rotations of all tiles
//         );
//
//         wfc_run(wfc, -1);    // Run Wave Function Collapse
//                              // -1 means no limit on iterations
//         struct wfc_image *output_image = wfc_output_image(wfc);
//         wfc_destroy(wfc);
//         // use output_image->data
//         // wfc_img_destroy(output_image);
//
// By default you work with struct wfc_image for inputs and outputs:
//
// struct wfc_image {
//         unsigned char *data;
//         int component_cnt;
//         int width;
//         int height;
// }
//
// Data is tightly packed without padding. Each pixel consists of
// component_cnt components (e.g., four components for rgba format).
// The output image will have the same number of components as the input
// image.
//
// wfc_run returns 0 if it cannot find a solution. You can try again like so:
//
//         wfc_init(wfc);
//         wfc_run(wfc, -1);
//
//
// Working with image files
// ----------------------------------------
//
// wfc can optionally use stb_image.h and stb_write.h to provide
// convenience functions for working directly with image files.
//
// You will normally place stb_image.h and stb_write.h in the same
// directory as wfc.h and include their implementations in one of the
// project files:
//
//         #define STB_IMAGE_IMPLEMENTATION
//         #define STB_IMAGE_WRITE_IMPLEMENTATION
//         #include "stb_image.h"
//         #include "stb_image_write.h"
//
// Further, you will instruct wfc.h to use stb:
//
//         #define WFC_IMPLEMENTATION
//         #define WFC_USE_STB
//         #include "wfc.h"
//
// Usage:
//
//         struct wfc_image *input_image = wfc_img_load("input.png");
//         struct wfc *wfc = wfc_overlapping(
//             ...
//             input_image,
//             ...
//         );
//
//         wfc_run(wfc, -1);    // Run Wave Function Collapse
//                              // -1 means no restriction on number of
//                              iterations
//         wfc_export(wfc, "output.png");
//         wfc_img_destroy(input_image);
//         wfc_destroy(wfc);
//
//
// Extra functions enabled by the inclusion of stb:
//
//         struct wfc_image *image = wfc_img_load("image.png")
//         wfc_img_save(image, "image.png")
//         wfc_export(wfc, "output.png")
//         wfc_export_tiles(wfc, "directory")
//         // don't forget to wfc_img_destroy(image) loaded images
//
#ifndef WFC_H
#define WFC_H

struct wfc;

struct wfc_image {
  unsigned char *data;
  int component_cnt;
  int width;
  int height;
};

struct wfc *wfc_overlapping(
    int output_width,         // Output width in pixels
    int output_height,        // Output height in pixels
    struct wfc_image *image,  // Input image to be cut into tiles
    int tile_width,           // Tile width in pixels
    int tile_height,          // Tile height in pixels
    int expand_input,         // Wrap input image on right and bottom
    int xflip_tiles,          // Add xflips of all tiles
    int yflip_tiles,          // Add yflips of all tiles
    int rotate_tiles);        // Add n*90deg rotations of all tiles

struct wfc_image *wfc_img_load(const char *filename);
void wfc_init(
    struct wfc *wfc);  // Resets wfc generation, wfc_run can be called again
int wfc_run(struct wfc *wfc, int max_collapse_cnt);
struct wfc_image *wfc_output_image(struct wfc *wfc);
int wfc_export(struct wfc *wfc, const char *filename);

void wfc_img_destroy(struct wfc_image *image);
void wfc_destroy(struct wfc *wfc);

#ifdef WFC_DEBUG

static const char *wfc__direction_strings[4] = {"up", "down", "left", "right"};

static void wfc__print_prop(struct wfc__prop *p, const char *prefix) {
  printf("%s%d -> %s -> %d\n", prefix, p->src_cell_idx,
         direction_strings[p->direction], p->dst_cell_idx);
}

static void wfc__print_props(struct wfc__prop *p, int prop_cnt,
                             const char *prefix) {
  for (int i = 0; i < prop_cnt; i++) {
    print_prop(&(p[i]), prefix);
  }
}

#define print_progress(int_progress)                    \
  do {                                                  \
    printf("\rcells collapsed:      %d", int_progress); \
    fflush(stdout);                                     \
  } while (0)

#define print_endprogress() printf("\n")
#define wfcassert(test) assert(test)
#define p(...) printf(__VA_ARGS__)

#else

#define print_progress(int_progress)
#define print_endprogress()
#define wfcassert(test)
#define p(...)

#endif  // WFC_DEBUG

#endif  // WFC_H
