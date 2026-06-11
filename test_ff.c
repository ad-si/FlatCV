#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "floodfill.h"

int main(void) {
  uint32_t width = 5;
  uint32_t height = 5;

  uint8_t data[25] = {
    10, 11, 14, 30, 30,
    9,  12, 13, 29, 31,
    8,  10, 16, 28, 32,
    7,  9,  25, 40, 40,
    7,  8,  26, 40, 40
  };

  Point2D marker = {.x = 0.0, .y = 0.0};

  uint8_t *result = fcv_flood_fill(width, height, data, &marker, 3, 9);
  if (!result) {
    printf("flood fill returned NULL\n");
    return 1;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      printf("%3u ", result[y * width + x]);
    }
    printf("\n");
  }

  free(result);
  return 0;
}
